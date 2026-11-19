using System.Text.RegularExpressions;
using System.Xml;
using System.Xml.Linq;

namespace ServerDependencyAnalyzer;

internal record CompileInput(string File, string Directory, string Standard,
    string[] Includes, string[] Defines, string[] ForcedIncludes, DatabaseFlags? Database = null, string? Project = null);

internal static class ProjectFiles
{
    private static readonly HashSet<string> ExcludedDirectories = new(StringComparer.OrdinalIgnoreCase)
    {
        ".git", ".vs", ".hg", ".svn", ".claude", ".codex", "bin", "obj", "out", "build", "CMakeFiles"
    };
    private static readonly HashSet<string> TestDirectories = new(StringComparer.OrdinalIgnoreCase)
    {
        "test", "tests", "unittest", "unittests", "integrationtest", "integrationtests"
    };

    private static readonly HashSet<string> VendorDirectories = new(StringComparer.OrdinalIgnoreCase)
    { "external", "third_party", "third-party", "thirdparty", "vendor", "packages", "node_modules" };

    private static readonly HashSet<string> BenchmarkDirectories = new(StringComparer.OrdinalIgnoreCase)
    { "benchmark", "benchmarks" };

    public static string[] Discover(string root, bool includeTests = false)
    {
        var solutionProjects = Directory.EnumerateFiles(root, "*.sln", SearchOption.TopDirectoryOnly)
            .SelectMany(solution => File.ReadLines(solution).Select(line =>
                Regex.Match(line, """^\s*Project\("[^"]+"\)\s*=\s*"[^"]*"\s*,\s*"([^"]+\.vcxproj)"\s*,""",
                    RegexOptions.IgnoreCase))
                .Where(match => match.Success)
                .Select(match => Path.GetFullPath(match.Groups[1].Value, root)))
            .Where(path => Inside(root, path) && File.Exists(path)).ToArray();

        return (solutionProjects.Length > 0 ? solutionProjects : Walk(root, "*.vcxproj", includeTests))
            .Where(path =>
            {
                var relative = Path.GetRelativePath(root, path);
                return !IsBenchmarkPath(relative) && ExternalPackage(relative) is null
                    && (includeTests || !IsTestPath(relative));
            }).Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(path => path, StringComparer.OrdinalIgnoreCase).ToArray();
    }

    public static List<CompileInput> Read(string root, string? entry, string configuration, string platform,
        bool includeTests, AnalysisResult result, CancellationToken cancellation, bool includeProjectReferences = true)
    {
        var files = new List<CompileInput>();
        if (BenchmarkDirectories.Contains(Path.GetFileName(Path.TrimEndingDirectorySeparator(root))))
            return files;
        if (!includeTests && TestDirectories.Contains(Path.GetFileName(Path.TrimEndingDirectorySeparator(root))))
        {
            result.ExcludedTests.Add(".");
            return files;
        }
        var projects = entry is null ? Discover(root, includeTests) : [Path.GetFullPath(entry, root)];
        var visited = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var project in projects)
            Visit(project);

        if (projects.Length == 0)
        {
            result.Notes.Add(new(".", "빌드 설정을 찾지 못해 C17/C++20 기본 설정으로 분석합니다."));
            var candidates = Walk(root, "*", includeTests).Where(path => IsSource(path) || IsHeader(path))
                .Where(path => !IsBenchmarkPath(Path.GetRelativePath(root, path))
                    && (includeTests || !ExcludeTest(root, path, result))).ToArray();
            var sources = candidates.Where(IsSource).ToArray();
            foreach (var source in sources.Length > 0 ? sources : candidates)
            {
                cancellation.ThrowIfCancellationRequested();
                files.Add(new(source, Path.GetDirectoryName(source)!, "c++20", [root, Path.GetDirectoryName(source)!], [], []));
            }
        }
        return files;

        void Visit(string project)
        {
            cancellation.ThrowIfCancellationRequested();
            var relativeProject = Path.GetRelativePath(root, project);
            if (!visited.Add(project) || IsBenchmarkPath(relativeProject) || ExternalPackage(relativeProject) is not null
                || (!includeTests && ExcludeTest(root, project, result)))
                return;

            try
            {
                var document = XDocument.Load(project, LoadOptions.SetLineInfo);
                var projectDirectory = Path.GetDirectoryName(project)!;
                var relative = Path.GetRelativePath(root, project);
                var id = ProjectId(relative);
                result.Nodes[id] = new(id, Path.GetFileNameWithoutExtension(project), "project", null, new(relative, 1));
                if (document.Root is not { } definition)
                    return;

                var properties = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
                {
                    ["SolutionDir"] = WithSeparator(FindSolutionDirectory(root, projectDirectory)),
                    ["ProjectDir"] = WithSeparator(projectDirectory),
                    ["MSBuildProjectDirectory"] = projectDirectory,
                    ["MSBuildThisFileDirectory"] = WithSeparator(projectDirectory),
                    ["ProjectName"] = Path.GetFileNameWithoutExtension(project),
                    ["Configuration"] = configuration,
                    ["Platform"] = platform
                };
                var unsupported = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                string Expand(string value) => Regex.Replace(value, @"\$\(([^)]+)\)", match =>
                    properties.TryGetValue(match.Groups[1].Value, out var replacement) ? replacement : match.Value);
                ReadVisualStudioFilters(root, project, id, Expand, result);
                bool Active(XElement element)
                {
                    var condition = ((string?)element.Attribute("Condition"))?.Trim();
                    if (string.IsNullOrEmpty(condition))
                        return true;

                    var comparison = Regex.Match(Expand(condition), """^['"]([^'"]*)['"]\s*(==|!=)\s*['"]([^'"]*)['"]$""");
                    if (comparison.Success && !comparison.Value.Contains("$(", StringComparison.Ordinal))
                        return comparison.Groups[1].Value.Equals(comparison.Groups[3].Value, StringComparison.OrdinalIgnoreCase)
                            == (comparison.Groups[2].Value == "==");

                    unsupported.Add($"조건을 평가하지 못했습니다. {condition}");
                    return false;
                }

                foreach (var group in definition.Elements().Where(element => element.Name.LocalName == "PropertyGroup" && Active(element)))
                    foreach (var property in group.Elements().Where(Active))
                        if (property.Name.LocalName is not ("Configuration" or "Platform"))
                            properties[property.Name.LocalName] = Expand(property.Value.Trim());

                var configurations = definition.Descendants().Where(element => element.Name.LocalName == "ProjectConfiguration")
                    .Select(element => (string?)element.Attribute("Include")).ToArray();
                if (configurations.Length > 0 && !configurations.Contains($"{configuration}|{platform}", StringComparer.OrdinalIgnoreCase))
                    result.Notes.Add(new(relative, $"선택한 구성 {configuration}|{platform}이 프로젝트에 없습니다."));

                var defaults = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                void ApplyMetadata(IEnumerable<XElement> elements, Dictionary<string, string> metadata)
                {
                    foreach (var element in elements.Where(Active))
                    {
                        var name = element.Name.LocalName;
                        var inherited = metadata.GetValueOrDefault(name, "");
                        metadata[name] = Expand(element.Value.Trim()).Replace($"%({name})", inherited,
                            StringComparison.OrdinalIgnoreCase);
                    }
                }

                foreach (var group in definition.Elements().Where(element => element.Name.LocalName == "ItemDefinitionGroup" && Active(element)))
                    foreach (var compile in group.Elements().Where(element => element.Name.LocalName == "ClCompile" && Active(element)))
                        ApplyMetadata(compile.Elements(), defaults);

                foreach (var item in definition.Elements().Where(element => element.Name.LocalName == "ItemGroup" && Active(element))
                    .SelectMany(group => group.Elements()).Where(element => element.Name.LocalName is "ClCompile" or "ProjectReference" && Active(element)))
                {
                    if (item.Name.LocalName == "ProjectReference" && !includeProjectReferences)
                        continue;
                    var include = Expand(((string?)item.Attribute("Include")) ?? "");
                    if (include.Length == 0)
                        continue;
                    if (include.Contains("$(", StringComparison.Ordinal) || include.IndexOfAny(['*', '?', ';']) >= 0)
                    {
                        unsupported.Add($"파일 항목을 해석하지 못했습니다. {include}");
                        continue;
                    }

                    var fullPath = Path.GetFullPath(include, projectDirectory);
                    var relativeFile = Path.GetRelativePath(root, fullPath);
                    if (IsBenchmarkPath(relativeFile) || ExternalPackage(relativeFile) is not null)
                        continue;
                    if (!includeTests && ExcludeTest(root, fullPath, result))
                        continue;
                    if (!Inside(root, fullPath) || !File.Exists(fullPath))
                    {
                        unsupported.Add($"폴더 안에서 파일을 찾지 못했습니다. {include}");
                        continue;
                    }

                    if (item.Name.LocalName == "ProjectReference")
                    {
                        Visit(fullPath);
                        var target = ProjectId(Path.GetRelativePath(root, fullPath));
                        if (result.Nodes.ContainsKey(target))
                            result.Connect(id, target, "reference", new(new(relative, Line(item)), include));
                        continue;
                    }

                    var settings = new Dictionary<string, string>(defaults, StringComparer.OrdinalIgnoreCase);
                    ApplyMetadata(item.Elements(), settings);
                    if (settings.GetValueOrDefault("ExcludedFromBuild")?.Equals("true", StringComparison.OrdinalIgnoreCase) == true)
                        continue;

                    string[] Values(string key)
                    {
                        var values = settings.GetValueOrDefault(key, "").Split(';',
                            StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
                        foreach (var value in values.Where(value => value.Contains("$(", StringComparison.Ordinal)))
                            unsupported.Add($"설정 {key}의 값을 해석하지 못했습니다. {value}");
                        return values.Where(value => !value.Contains("$(", StringComparison.Ordinal) &&
                            !value.Contains("%(", StringComparison.Ordinal)).ToArray();
                    }

                    var standard = settings.GetValueOrDefault("LanguageStandard") switch
                    {
                        "stdcpp17" => "c++17", "stdcpp20" => "c++20", "stdcpp23" or "stdcpp23preview" => "c++23",
                        "stdcpplatest" => "c++2c", _ => "c++14"
                    };
                    var defines = Values("PreprocessorDefinitions").ToList();
                    if (properties.GetValueOrDefault("CharacterSet") == "Unicode")
                        defines.AddRange(["UNICODE", "_UNICODE"]);
                    var includes = Values("AdditionalIncludeDirectories")
                        .Select(path => Path.GetFullPath(path.Trim('"'), projectDirectory)).Append(projectDirectory).Distinct().ToArray();
                    files.Add(new(fullPath, projectDirectory, standard, includes,
                        defines.Distinct().ToArray(), Values("ForcedIncludeFiles"), Project: id));
                }

                foreach (var import in definition.Descendants().Where(element => element.Name.LocalName == "Import"))
                {
                    var path = (string?)import.Attribute("Project") ?? "";
                    if (!path.StartsWith("$(VCTargetsPath)", StringComparison.OrdinalIgnoreCase)
                        && !path.StartsWith("$(UserRootDir)", StringComparison.OrdinalIgnoreCase))
                        unsupported.Add($"외부 빌드 설정을 적용하지 않았습니다. {path}");
                }
                result.Notes.AddRange(unsupported.Select(message => new AnalysisIssue(relative, message)));
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or XmlException or ArgumentException)
            {
                result.Failures.Add(new(Path.GetRelativePath(root, project), exception.Message));
            }
        }
    }

    private static void ReadVisualStudioFilters(string root, string project, string id,
        Func<string, string> expand, AnalysisResult result)
    {
        var path = project + ".filters";
        if (!File.Exists(path))
            return;
        var directory = Path.GetDirectoryName(project)!;
        try
        {
            var document = XDocument.Load(path);
            foreach (var item in document.Root?.Elements().SelectMany(group => group.Elements()) ?? [])
            {
                if (item.Name.LocalName is not ("ClInclude" or "ClCompile"))
                    continue;
                var input = expand((string?)item.Attribute("Include") ?? "");
                var name = item.Elements().FirstOrDefault(element => element.Name.LocalName == "Filter")?.Value.Trim();
                if (string.IsNullOrEmpty(name) || input.Length == 0 || input.Contains("$(", StringComparison.Ordinal)
                    || input.IndexOfAny(['*', '?', ';']) >= 0)
                    continue;
                var file = Path.GetFullPath(input, directory);
                if (!Inside(root, file))
                    continue;
                var relative = Path.GetRelativePath(root, file);
                if (!result.StudioFilters.TryGetValue(relative, out var filters))
                    result.StudioFilters.Add(relative, filters = []);
                if (filters.All(filter => filter.Project != id))
                    filters.Add(new(id, name.Replace('/', '\\')));
            }
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or XmlException or ArgumentException)
        {
            result.Notes.Add(new(Path.GetRelativePath(root, path), $"Visual Studio 필터를 읽지 못했습니다. {exception.Message}"));
        }
    }

    public static bool Inside(string root, string path)
    {
        var relative = Path.GetRelativePath(root, path);
        return !Path.IsPathRooted(relative) && relative != ".." && !relative.StartsWith(@"..\", StringComparison.Ordinal);
    }

    private static string FindSolutionDirectory(string root, string directory)
    {
        for (var current = new DirectoryInfo(directory); current is not null && Inside(root, current.FullName); current = current.Parent)
            if (current.EnumerateFiles("*.sln", SearchOption.TopDirectoryOnly).Any())
                return current.FullName;
        return root;
    }

    private static string ProjectId(string relative) => "project:" + relative.Replace('\\', '/').ToUpperInvariant();
    private static uint Line(XElement element) => (uint)((IXmlLineInfo)element).LineNumber;
    private static string WithSeparator(string path) => Path.TrimEndingDirectorySeparator(path) + "\\";
    private static bool IsSource(string path) => Path.GetExtension(path).ToLowerInvariant() is ".cpp" or ".cxx" or ".cc" or ".c" or ".c++";
    private static bool IsHeader(string path) => Path.GetExtension(path).ToLowerInvariant() is ".h" or ".hpp" or ".hxx" or ".hh";

    public static (string Name, string Path)? ExternalPackage(string relativePath)
    {
        var parts = relativePath.Split(['\\', '/']);
        for (var index = 0; index + 1 < parts.Length; index++)
        {
            if (!VendorDirectories.Contains(parts[index]))
                continue;
            var package = parts[index + 1];
            return (index + 2 == parts.Length ? Path.GetFileNameWithoutExtension(package) : package,
                string.Join(Path.DirectorySeparatorChar, parts.Take(index + 2)));
        }
        return null;
    }

    public static bool IsBenchmarkPath(string relativePath)
    {
        var parts = relativePath.Split(['\\', '/']);
        var name = Path.GetFileNameWithoutExtension(parts[^1]);
        return parts.Take(parts.Length - 1).Any(BenchmarkDirectories.Contains)
            || BenchmarkDirectories.Contains(name) || name.EndsWith("Benchmark", StringComparison.Ordinal)
            || name.EndsWith("_benchmark", StringComparison.OrdinalIgnoreCase);
    }

    public static bool IsTestPath(string relativePath)
    {
        var segments = relativePath.Split(['\\', '/']);
        if (segments.Take(segments.Length - 1).Any(TestDirectories.Contains))
            return true;
        var name = Path.GetFileNameWithoutExtension(segments[^1]);
        return TestDirectories.Contains(name)
            || name.StartsWith("IntegrationTest", StringComparison.Ordinal)
            || name.StartsWith("UnitTest", StringComparison.Ordinal)
            || name.EndsWith("Test", StringComparison.Ordinal)
            || name.EndsWith("Tests", StringComparison.Ordinal)
            || (name.StartsWith("Test", StringComparison.Ordinal) && name.Length > 4 &&
                (char.IsUpper(name[4]) || name[4] is '_' or '.' or '-'))
            || Regex.IsMatch(name, @"[._-]tests?$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
    }

    public static bool ExcludeTest(string root, string path, AnalysisResult result)
    {
        var relative = Path.GetRelativePath(root, path);
        if (!IsTestPath(relative))
            return false;
        result.ExcludedTests.Add(relative);
        return true;
    }

    private static IEnumerable<string> Walk(string root, string pattern, bool includeTests)
    {
        var pending = new Stack<string>();
        pending.Push(root);
        var options = new EnumerationOptions { IgnoreInaccessible = true, AttributesToSkip = FileAttributes.ReparsePoint };
        while (pending.TryPop(out var current))
        {
            foreach (var file in Directory.EnumerateFiles(current, pattern, options))
                yield return file;
            foreach (var directory in Directory.EnumerateDirectories(current, "*", options))
            {
                var name = Path.GetFileName(directory);
                if (ExcludedDirectories.Contains(name) || VendorDirectories.Contains(name) || BenchmarkDirectories.Contains(name)
                    || name.StartsWith("cmake-build-", StringComparison.OrdinalIgnoreCase)
                    || (!includeTests && TestDirectories.Contains(name)))
                    continue;
                var gitMarker = Path.Combine(directory, ".git");
                if (File.Exists(gitMarker) || Directory.Exists(gitMarker))
                    continue;
                if (UnityGenerated.Contains(name) && File.Exists(Path.Combine(current, "ProjectSettings", "ProjectVersion.txt")))
                    continue;
                pending.Push(directory);
            }
        }
    }

    private static readonly HashSet<string> UnityGenerated = new(StringComparer.OrdinalIgnoreCase)
    { "Library", "Temp", "Logs", "UserSettings" };
}

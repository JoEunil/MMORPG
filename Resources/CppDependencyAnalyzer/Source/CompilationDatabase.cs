using System.Runtime.InteropServices;
using System.Text.Json;

namespace ServerDependencyAnalyzer;

internal record DatabaseFlags(string Target, bool UseMsvcHeaders, string[] Arguments);

internal static class CompilationDatabase
{
    private const string Name = "compile_commands.json";
    private static readonly EnumerationOptions Browse = new()
    {
        IgnoreInaccessible = true, AttributesToSkip = FileAttributes.ReparsePoint
    };
    private static readonly string[] IncludeOptions = ["-idirafter", "-isystem", "-iquote", "-I"];
    private static readonly string[] MsvcIncludeOptions = ["/external:I", "/imsvc", "/FI", "/I", "-I"];
    private static readonly HashSet<string> LanguageOptions = new(StringComparer.Ordinal)
    {
        "-fms-extensions", "-fno-ms-extensions", "-fms-compatibility", "-fno-ms-compatibility",
        "-fdelayed-template-parsing", "-fno-delayed-template-parsing", "-fshort-wchar", "-fshort-enums",
        "-fexceptions", "-fno-exceptions", "-frtti", "-fno-rtti", "-funsigned-char", "-fsigned-char",
        "-fdeclspec", "-fopenmp", "-pthread", "-m32", "-m64", "-nostdinc", "-nostdinc++", "-nostdlibinc",
        "-O0", "-O1", "-O2", "-O3", "-Og", "-Os", "-Oz", "-Ofast"
    };

    public static bool IsDatabase(string? path) => string.Equals(Path.GetExtension(path), ".json", StringComparison.OrdinalIgnoreCase);

    public static string[] Discover(string root)
    {
        var locations = new List<string>();
        AddIfPresent(root);
        foreach (var build in Directory.EnumerateDirectories(root, "*", Browse).Where(IsBuildFolder))
            Walk(build, 2);
        return locations.OrderBy(path => Path.GetRelativePath(root, path).Count(character => character is '\\' or '/'))
            .ThenBy(path => path, StringComparer.OrdinalIgnoreCase).ToArray();

        void AddIfPresent(string directory)
        {
            var file = Path.Combine(directory, Name);
            if (File.Exists(file))
                locations.Add(file);
        }

        void Walk(string directory, int depth)
        {
            AddIfPresent(directory);
            if (depth == 0)
                return;
            foreach (var child in Directory.EnumerateDirectories(directory, "*", Browse))
                if (!Path.GetFileName(child).Equals("CMakeFiles", StringComparison.OrdinalIgnoreCase))
                    Walk(child, depth - 1);
        }
    }

    public static List<CompileInput> Read(string root, string path, bool includeTests,
        AnalysisResult result, CancellationToken cancellation)
    {
        var items = new List<CompileInput>();
        using var stream = File.OpenRead(path);
        using var document = JsonDocument.Parse(stream);
        if (document.RootElement.ValueKind != JsonValueKind.Array)
            throw new JsonException("컴파일 데이터베이스의 최상위 값은 배열이어야 합니다.");

        var variants = new Dictionary<string, HashSet<string>>(StringComparer.OrdinalIgnoreCase);
        var ignored = new SortedSet<string>(StringComparer.Ordinal);
        var row = 0;
        var outside = 0;
        foreach (var entry in document.RootElement.EnumerateArray())
        {
            cancellation.ThrowIfCancellationRequested();
            row++;
            try
            {
                var directory = Path.GetFullPath(entry.GetProperty("directory").GetString()!, Path.GetDirectoryName(path)!);
                var file = Path.GetFullPath(entry.GetProperty("file").GetString()!, directory);
                if (!ProjectFiles.Inside(root, file))
                {
                    outside++;
                    continue;
                }
                var relative = Path.GetRelativePath(root, file);
                if (ProjectFiles.IsBenchmarkPath(relative) || ProjectFiles.ExternalPackage(relative) is not null)
                    continue;
                if (!includeTests && ProjectFiles.ExcludeTest(root, file, result))
                    continue;

                var arguments = entry.TryGetProperty("arguments", out var argv)
                    ? argv.EnumerateArray().Select(argument => argument.GetString()
                        ?? throw new JsonException("컴파일 인자는 문자열이어야 합니다.")).ToArray()
                    : SplitCommand(entry.GetProperty("command").GetString()!);
                if (arguments.Length == 0)
                    throw new JsonException("컴파일 명령이 비어 있습니다.");
                if (!File.Exists(file))
                    throw new FileNotFoundException("소스 파일을 찾지 못했습니다.", file);

                var flags = ReadFlags(arguments, file, directory, ignored);
                if (!variants.TryGetValue(file, out var signatures))
                    variants.Add(file, signatures = new(StringComparer.Ordinal));
                if (signatures.Add(string.Join('\0', flags.Arguments.Prepend(flags.Target))))
                    items.Add(new(file, directory, "c++20", [], [], [], flags));
            }
            catch (Exception exception) when (exception is ArgumentException or InvalidOperationException or
                KeyNotFoundException or IOException or JsonException)
            {
                result.Failures.Add(new($"{Path.GetFileName(path)} #{row}", exception.Message));
            }
        }

        if (ignored.Count > 0)
            result.Notes.Add(new(Path.GetFileName(path), $"적용하지 않은 컴파일 옵션 {string.Join(", ", ignored.Take(24))}"));
        if (outside > 0)
            result.Notes.Add(new(Path.GetFileName(path), $"선택한 소스 폴더 바깥의 컴파일 항목 {outside}개를 제외했습니다."));
        if (variants.Values.Any(value => value.Count > 1))
            result.Notes.Add(new(Path.GetFileName(path), "같은 소스의 서로 다른 컴파일 설정을 각각 분석해 관계를 합쳤습니다."));
        return items;
    }

    private static DatabaseFlags ReadFlags(string[] arguments, string file, string directory, ISet<string> ignored)
    {
        var compilerIndex = 0;
        while (compilerIndex < arguments.Length - 1 && CompilerName(arguments[compilerIndex]) is "ccache" or "sccache")
            compilerIndex++;
        var compiler = CompilerName(arguments[compilerIndex]);
        var msvc = compiler is "cl" or "clang-cl" || arguments.Contains("--driver-mode=cl", StringComparer.Ordinal);
        var mingw = compiler.Contains("mingw", StringComparison.Ordinal) || compiler is "gcc" or "g++";
        var language = Path.GetExtension(file).Equals(".c", StringComparison.OrdinalIgnoreCase) ? "c" : "c++";
        var target = mingw ? MingwTarget(compiler) : NativeTarget(arguments[compilerIndex]);
        var output = new List<string>();
        var explicitTarget = false;

        for (var index = compilerIndex + 1; index < arguments.Length; index++)
        {
            var arg = arguments[index];
            if (msvc && arg is "-MD" or "-MDd" or "-MT" or "-MTd")
                arg = "/" + arg[1..];
            string Next() => index + 1 < arguments.Length ? arguments[++index] : "";
            string Value(string option) => arg.Length == option.Length ? Next() : arg[option.Length..];
            void AddValue(string argument, string value)
            {
                if (!string.IsNullOrEmpty(value))
                    output.Add(argument + value);
            }
            void Include(string option, string value, bool search = false)
            {
                if (value.Length == 0)
                    return;
                var fullPath = Path.GetFullPath(value, directory);
                output.Add(option);
                output.Add(search && !File.Exists(fullPath) && !Path.IsPathRooted(value) && !value.StartsWith('@') && !value.StartsWith('-')
                    ? value : fullPath);
            }

            if (arg.Length == 0 || IsInput(arg, file, directory) || arg is "-c" or "/c" or "/nologo" or "/showIncludes")
                continue;
            if (arg.Equals("/link", StringComparison.OrdinalIgnoreCase))
                break;
            if (arg is "-o" or "-MF" or "-MT" or "-MQ" or "-MJ" or "/Fo" or "/Fd" or "/Fe" or "/Fa")
                _ = Next();
            else if (arg is "-Xclang" or "-Xpreprocessor" or "-Xassembler" or "-Xlinker" or "-load" or "-fplugin" or "--config")
            {
                ignored.Add(arg);
                _ = Next();
            }
            else if (arg.StartsWith('@'))
                ignored.Add("@response-file");
            else if (arg is "--target" or "-target" || arg.StartsWith("--target=", StringComparison.Ordinal))
            {
                var value = arg.Contains('=') ? arg[(arg.IndexOf('=') + 1)..] : Next();
                if (DriverToken(value))
                {
                    target = value;
                    explicitTarget = true;
                }
            }
            else if (arg == "-x")
            {
                var value = Next();
                if (value is "c" or "c++" or "c-header" or "c++-header")
                    language = value;
                else
                    ignored.Add("-x " + value);
            }
            else if (msvc && (arg is "/TC" or "/TP" || arg.StartsWith("/Tc", StringComparison.Ordinal) || arg.StartsWith("/Tp", StringComparison.Ordinal)))
            {
                if (arg is "/Tc" or "/Tp")
                    _ = Next();
                language = arg.StartsWith("/TC", StringComparison.OrdinalIgnoreCase) ? "c" : "c++";
            }
            else if (arg.StartsWith("-std=", StringComparison.Ordinal) || (msvc &&
                (arg.StartsWith("/std:", StringComparison.OrdinalIgnoreCase) || arg.StartsWith("-std:", StringComparison.Ordinal))))
            {
                var standard = arg[5..];
                if (DriverToken(standard))
                    AddValue("-std=", standard == "c++latest" ? "c++2c" : standard);
            }
            else if (arg.StartsWith("-D", StringComparison.Ordinal) || (msvc && arg.StartsWith("/D", StringComparison.Ordinal)))
                AddValue("-D", Value(arg[..2]));
            else if (arg.StartsWith("-U", StringComparison.Ordinal) || (msvc && arg.StartsWith("/U", StringComparison.Ordinal)))
                AddValue("-U", Value(arg[..2]));
            else if (msvc && MsvcIncludeOptions.FirstOrDefault(option => arg.StartsWith(option, StringComparison.Ordinal)) is { } winInclude)
                Include(winInclude is "/I" or "-I" ? "-I" : winInclude == "/FI" ? "-include" : "-isystem",
                    Value(winInclude), winInclude == "/FI");
            else if (IncludeOptions.FirstOrDefault(option => arg.StartsWith(option, StringComparison.Ordinal)) is { } include)
                Include(include, Value(include));
            else if (arg is "-include" or "-imacros" or "-isysroot")
                Include(arg, Next(), arg is "-include" or "-imacros");
            else if (arg is "--sysroot" or "--gcc-toolchain")
                AddValue(arg + "=", Path.GetFullPath(Next(), directory));
            else if (arg.StartsWith("--sysroot=", StringComparison.Ordinal) || arg.StartsWith("--gcc-toolchain=", StringComparison.Ordinal))
                AddValue(arg[..(arg.IndexOf('=') + 1)], Path.GetFullPath(arg[(arg.IndexOf('=') + 1)..], directory));
            else if (LanguageOptions.Contains(arg) || IsArchitectureOption(arg))
                output.Add(arg);
            else if (msvc && arg is "/MD" or "/MDd" or "/MT" or "/MTd")
            {
                output.Add("-D_MT");
                if (arg.StartsWith("/MD", StringComparison.Ordinal)) output.Add("-D_DLL");
                if (arg.EndsWith('d')) output.Add("-D_DEBUG");
            }
            else if (msvc && arg is "/GR" or "/GR-")
                output.Add(arg == "/GR" ? "-frtti" : "-fno-rtti");
            else if (msvc && arg is "/EHsc" or "/EHs" or "/EHa")
                output.Add("-fexceptions");
            else if (msvc && arg is "/EHs-c-" or "/EHsc-")
                output.Add("-fno-exceptions");
            else if (!BuildOnly(arg))
                ignored.Add(arg.Split('=', ':')[0]);
        }

        if (!explicitTarget && output.Contains("-m32") && target.StartsWith("x86_64", StringComparison.Ordinal))
            target = "i686" + target[6..];
        var native = target.Contains("msvc", StringComparison.OrdinalIgnoreCase)
            && !output.Any(argument => argument is "-nostdinc" or "-nostdlibinc" or "-isysroot"
                || argument.StartsWith("--sysroot=", StringComparison.Ordinal));
        if (!output.Any(argument => argument.StartsWith("-std=", StringComparison.Ordinal)))
            output.Insert(0, language.StartsWith("c++", StringComparison.Ordinal) ? "-std=c++17" : "-std=c17");
        output.InsertRange(0, ["-working-directory", directory, "-x", language]);
        return new(target, native, output.ToArray());
    }

    private static bool DriverToken(string value) => value.Length > 0 &&
        value.All(character => char.IsAsciiLetterOrDigit(character) || character is '+' or '-' or '_' or '.');

    private static bool IsArchitectureOption(string arg) => new[] { "-arch=", "-march=", "-mcpu=", "-mtune=", "-mfloat-abi=" }
        .Any(prefix => arg.StartsWith(prefix, StringComparison.Ordinal) && DriverToken(arg[prefix.Length..]));

    private static bool IsInput(string arg, string source, string directory) =>
        arg.Length > 0 && !arg.StartsWith('-') && !arg.StartsWith('@') &&
        Path.GetFileName(arg).Equals(Path.GetFileName(source), StringComparison.OrdinalIgnoreCase) &&
        Path.GetFullPath(arg, directory).Equals(source, StringComparison.OrdinalIgnoreCase);

    private static bool BuildOnly(string arg) => arg is "-MD" or "-MMD" or "-MP" or "-g" or "-w" or "-pipe" or "/FS" or "--driver-mode=cl"
        || new[] { "-W", "-fdiagnostics-", "/Fo", "/Fd", "/Fe", "/Fa", "/W", "/w", "/Zi", "/Z7", "/EH", "/RTC", "/O", "/nologo", "/utf-8", "/permissive", "/Zc:" }
            .Any(prefix => arg.StartsWith(prefix, StringComparison.Ordinal));

    private static string CompilerName(string path)
    {
        var name = Path.GetFileName(path).ToLowerInvariant();
        return name.EndsWith(".exe", StringComparison.Ordinal) ? name[..^4] : name;
    }

    private static string MingwTarget(string compiler) =>
        compiler.StartsWith("i686", StringComparison.Ordinal) ? "i686-w64-windows-gnu"
        : compiler.StartsWith("aarch64", StringComparison.Ordinal) ? "aarch64-w64-windows-gnu" : "x86_64-w64-windows-gnu";

    private static string NativeTarget(string compiler) =>
        Path.GetFileName(Path.GetDirectoryName(compiler))?.ToLowerInvariant() switch
        {
            "x86" => "i686-pc-windows-msvc",
            "arm64" => "aarch64-pc-windows-msvc",
            _ => "x86_64-pc-windows-msvc"
        };

    private static bool IsBuildFolder(string path)
    {
        var name = Path.GetFileName(path);
        return name.StartsWith("build", StringComparison.OrdinalIgnoreCase) ||
            name.StartsWith("cmake-build-", StringComparison.OrdinalIgnoreCase) ||
            name.Equals("out", StringComparison.OrdinalIgnoreCase);
    }

    private static string[] SplitCommand(string command)
    {
        if (string.IsNullOrWhiteSpace(command))
            return [];
        var argv = CommandLineToArgvW(command, out var count);
        if (argv == 0)
            throw new JsonException("Windows 컴파일 명령의 인자를 읽지 못했습니다.");
        try
        {
            return Enumerable.Range(0, count)
                .Select(index => Marshal.PtrToStringUni(Marshal.ReadIntPtr(argv, index * IntPtr.Size)) ?? "").ToArray();
        }
        finally { _ = LocalFree(argv); }
    }

    [DllImport("shell32.dll", CharSet = CharSet.Unicode, ExactSpelling = true)]
    private static extern nint CommandLineToArgvW(string command, out int count);

    [DllImport("kernel32.dll", ExactSpelling = true)]
    private static extern nint LocalFree(nint memory);
}

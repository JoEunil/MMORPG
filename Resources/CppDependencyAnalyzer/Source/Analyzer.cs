using System.Diagnostics;
using System.Runtime.InteropServices;
using static ServerDependencyAnalyzer.ClangNative;

namespace ServerDependencyAnalyzer;

internal record AnalysisOptions(string Root, string? Entry, string Configuration, string Platform,
    string? SingleFile = null, bool IncludeTests = false, bool ShowExternalLibraries = true, bool DiscoverDatabase = true,
    InspectionOptions? Inspections = null);

internal static class Analyzer
{
    public static AnalysisResult Run(AnalysisOptions options, IProgress<string>? progress, CancellationToken cancellation)
    {
        var started = Stopwatch.GetTimestamp();
        var result = RunCore(options, progress, cancellation);
        result.ElapsedMilliseconds = Stopwatch.GetElapsedTime(started).TotalMilliseconds;
        return result;
    }

    private static AnalysisResult RunCore(AnalysisOptions options, IProgress<string>? progress, CancellationToken cancellation)
    {
        var root = Path.GetFullPath(options.Root);
        if (!Directory.Exists(root))
            throw new DirectoryNotFoundException(root);
        var entry = options.Entry is not null ? Path.GetFullPath(options.Entry, root)
            : options.DiscoverDatabase ? CompilationDatabase.Discover(root).FirstOrDefault() : null;
        var database = CompilationDatabase.IsDatabase(entry);
        if (entry is not null && !database && !ProjectFiles.Inside(root, entry))
            throw new ArgumentException("진입 프로젝트를 선택한 분석 폴더 안에서 선택하세요.");

        var result = new AnalysisResult(root, database
            ? (ProjectFiles.Inside(root, entry!) ? Path.GetRelativePath(root, entry!) : entry!)
            : $"{options.Configuration}|{options.Platform}");
        var inspections = options.Inspections is { Enabled: true } enabled ? new CodeInspections(enabled, cancellation) : null;
        if (inspections is not null)
            result.Inspections = InspectionReport.Create(options.Inspections);
        progress?.Report("프로젝트와 C++ 소스 파일을 찾는 중…");
        var allInputs = database ? CompilationDatabase.Read(root, entry!, options.IncludeTests, result, cancellation)
            : ProjectFiles.Read(root, entry, options.Configuration, options.Platform, options.IncludeTests, result, cancellation);
        var input = options.SingleFile is null ? allInputs : allInputs.Where(file =>
            file.File.Equals(Path.GetFullPath(options.SingleFile, root), StringComparison.OrdinalIgnoreCase)).ToList();
        result.Sources = input.Count;
        if (input.Count == 0)
        {
            result.Notes.Add(new(".", "분석할 C++ 소스 파일을 찾지 못했습니다."));
            return result;
        }

        ClangInstallation installation;
        try
        {
            installation = ClangInstallation.Find();
            Load(installation.Library);
            result.Notes.Add(new(".", Read(clang_getClangVersion())));
        }
        catch (Exception exception) when (exception is InvalidOperationException or DllNotFoundException
            or BadImageFormatException or EntryPointNotFoundException)
        {
            result.Failures.Add(new(".", exception.Message));
            return result;
        }

        if (installation.SystemIncludes.Length == 0 && input.Any(file => file.Database?.UseMsvcHeaders != false))
            result.Notes.Add(new(".", "Visual Studio C++ 및 Windows SDK 헤더 경로를 찾지 못했습니다."));
        if (installation.ResourceDirectory is null)
            result.Notes.Add(new(".", "Clang 내장 헤더가 필요하면 CLANG_RESOURCE_DIR을 지정하세요."));

        var index = clang_createIndex(0, 0);
        try
        {
            var showLibraries = options.ShowExternalLibraries && result.Nodes.Values.Any(node => node.Kind == "project");
            var graph = new AstGraph(root, result, options.IncludeTests, showLibraries, cancellation);
            const uint keepGoing = 0x200, detailedPreprocessingRecord = 0x01;
            var parseFlags = keepGoing | (showLibraries ? detailedPreprocessingRecord : 0);
            for (var number = 0; number < input.Count; number++)
            {
                cancellation.ThrowIfCancellationRequested();
                var file = input[number];
                var relative = Path.GetRelativePath(root, file.File);
                progress?.Report($"{number + 1}/{input.Count}  {relative}");
                var arguments = Arguments(file, options.Platform, installation);
                var pointers = new nint[arguments.Count];
                nint unit = 0;
                try
                {
                    for (var argument = 0; argument < pointers.Length; argument++)
                        pointers[argument] = Marshal.StringToCoTaskMemUTF8(arguments[argument]);
                    // KeepGoing preserves recoverable declarations so the report can show partial results.
                    var status = clang_parseTranslationUnit2(index, file.File, pointers, pointers.Length, 0, 0,
                        parseFlags, out unit);
                    if (status != 0 || unit == 0)
                    {
                        result.Failures.Add(new(relative, $"Clang이 구문 트리를 만들지 못했습니다. 상태 코드 {status}."));
                        continue;
                    }

                    cancellation.ThrowIfCancellationRequested();
                    var errors = ReadDiagnostics(unit);
                    if (errors.Count == 0)
                        result.Parsed++;
                    else
                    {
                        result.ParsedWithErrors++;
                        result.Failures.Add(new(relative, string.Join(Environment.NewLine, errors)));
                    }

                    graph.ReadUnit(unit, file, errors.Count == 0 ? inspections : null);
                }
                finally
                {
                    if (unit != 0)
                        clang_disposeTranslationUnit(unit);
                    foreach (var pointer in pointers)
                        Marshal.FreeCoTaskMem(pointer);
                }
            }
        }
        finally { clang_disposeIndex(index); }
        if (inspections is not null)
            result.Inspections = inspections.Finish();
        return result;
    }

    private static List<string> ReadDiagnostics(nint unit)
    {
        var diagnostics = new List<string>();
        var total = clang_getNumDiagnostics(unit);
        var display = clang_defaultDiagnosticDisplayOptions();
        for (uint item = 0; item < total && diagnostics.Count < 8; item++)
        {
            var diagnostic = clang_getDiagnostic(unit, item);
            try
            {
                if (clang_getDiagnosticSeverity(diagnostic) >= 3)
                    diagnostics.Add(Read(clang_formatDiagnostic(diagnostic, display)));
            }
            finally { clang_disposeDiagnostic(diagnostic); }
        }
        return diagnostics;
    }

    private static List<string> Arguments(CompileInput input, string platform, ClangInstallation installation)
    {
        var architecture = platform.ToUpperInvariant() switch { "WIN32" or "X86" => "i686", "ARM64" => "aarch64", _ => "x86_64" };
        var target = input.Database?.Target ?? $"{architecture}-pc-windows-msvc";
        var useMsvcHeaders = input.Database?.UseMsvcHeaders ?? true;
        var arguments = new List<string> { $"--target={target}", "-fsyntax-only", "-ferror-limit=20", "-w" };
        if (target.Contains("msvc", StringComparison.OrdinalIgnoreCase))
            arguments.AddRange(["-fms-extensions", "-fms-compatibility"]);
        if (target.Contains("msvc", StringComparison.OrdinalIgnoreCase) && installation.MsvcVersion is { } version)
            arguments.Add($"-fms-compatibility-version={version}");
        if (installation.ResourceDirectory is { } resource)
        {
            arguments.Add("-resource-dir");
            arguments.Add(resource);
        }
        if (input.Database is { } database)
            arguments.AddRange(database.Arguments);
        else
        {
            var isC = Path.GetExtension(input.File).Equals(".c", StringComparison.OrdinalIgnoreCase);
            arguments.AddRange(["-x", isC ? "c" : "c++", isC ? "-std=c17" : $"-std={input.Standard}"]);
            if (!isC)
                arguments.Add("-fdelayed-template-parsing");
            foreach (var include in input.Includes)
            {
                arguments.Add("-I");
                arguments.Add(include);
            }
            arguments.AddRange(input.Defines.Select(define => "-D" + define));
            foreach (var include in input.ForcedIncludes)
            {
                arguments.Add("-include");
                arguments.Add(include.StartsWith('@') ? Path.Combine(input.Directory, include) : include);
            }
        }
        if (useMsvcHeaders)
            foreach (var include in installation.SystemIncludes)
            {
                arguments.Add("-isystem");
                arguments.Add(include);
            }
        return arguments;
    }
}

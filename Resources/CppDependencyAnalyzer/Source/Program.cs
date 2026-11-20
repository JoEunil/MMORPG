using System.Diagnostics;

namespace ServerDependencyAnalyzer;

internal static class Program
{
    [STAThread]
    private static int Main(string[] arguments)
    {
        if (arguments.Length == 1 && arguments[0] is "--serve" or "--start-service" or "--stop-service")
        {
            try
            {
                if (arguments[0] == "--serve") ReanalysisService.Run().GetAwaiter().GetResult();
                else if (arguments[0] == "--start-service") ReanalysisService.EnsureRunning();
                else ReanalysisService.Stop();
                return 0;
            }
            catch (Exception exception) { Console.Error.WriteLine(exception.Message); return 1; }
        }
        if (arguments.Length > 0 && arguments[0] is "--register-vs" or "--open-source" or "--refresh-report")
        {
            try
            {
                if (arguments[0] == "--register-vs" && arguments.Length is 1 or 2)
                    VisualStudioLink.Register(arguments.Length == 2 ? arguments[1] : null);
                else if (arguments.Length == 2 && arguments[0] == "--open-source")
                {
                    VisualStudioLink.Open(arguments[1]);
                }
                else if (arguments.Length == 2 && arguments[0] == "--refresh-report") HtmlReport.Refresh(arguments[1]);
                else throw new ArgumentException("명령줄 인자를 확인하세요.");
                return 0;
            }
            catch (Exception exception)
            {
                Console.Error.WriteLine(exception.Message);
                if (arguments[0] == "--open-source") MessageBox.Show(exception.Message, "분석기 로컬 연결");
                return 1;
            }
        }
        if (arguments.Length > 0)
            return AnalyzeFromCommandLine(arguments);

        ApplicationConfiguration.Initialize();
        Application.Run(new AnalyzerForm());
        return 0;
    }

    private static int AnalyzeFromCommandLine(string[] arguments)
    {
        if (arguments.Length < 2 || arguments[0] != "--analyze" || arguments.Length % 2 != 0)
        {
            Console.Error.WriteLine("--analyze <폴더> [--entry <vcxproj|compile_commands.json>] [--configuration Debug] [--platform x64] [--output <HTML>] [--file <CPP>] [--include-tests true] [--include-vendor false]");
            Console.Error.WriteLine("프로젝트 범위 (기본 true) [--include-project-references true|false]");
            Console.Error.WriteLine("선택 검사 (기본 false) [--member-usage true|false] [--parameter-groups true|false] [--class-cohesion true|false]");
            Console.Error.WriteLine("                       [--responsibility-concentration true|false]");
            Console.Error.WriteLine("                       [--loop-copy true|false] [--loop-search true|false] [--loop-allocation true|false]");
            Console.Error.WriteLine("메서드 호출 지도 (기본 false) [--method-map true|false]");
            return 1;
        }

        using var cancellation = new CancellationTokenSource();
        Console.CancelKeyPress += (_, key) => { key.Cancel = true; cancellation.Cancel(); };
        try
        {
            var flags = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            for (var index = 2; index < arguments.Length; index += 2)
            {
                var flag = arguments[index];
                if (flag is not ("--entry" or "--configuration" or "--platform" or "--output" or "--file" or "--include-tests" or "--include-vendor" or "--include-project-references"
                    or "--member-usage" or "--parameter-groups" or "--class-cohesion" or "--responsibility-concentration"
                    or "--loop-copy" or "--loop-search" or "--loop-allocation" or "--method-map")
                    || !flags.TryAdd(flag, arguments[index + 1]))
                    throw new ArgumentException($"지원하지 않거나 중복된 옵션입니다. {flag}");
            }
            bool IsEnabled(string flag, bool defaultValue = false)
            {
                if (!flags.TryGetValue(flag, out var value))
                    return defaultValue;
                if (bool.TryParse(value, out var enabled))
                    return enabled;
                throw new ArgumentException($"{flag} 옵션에 true 또는 false를 입력하세요.");
            }

            var options = new AnalysisOptions(arguments[1], flags.GetValueOrDefault("--entry"),
                flags.GetValueOrDefault("--configuration", "Debug"), flags.GetValueOrDefault("--platform", "x64"),
                flags.GetValueOrDefault("--file"),
                IsEnabled("--include-tests"), IsEnabled("--include-vendor", defaultValue: true),
                Inspections: new InspectionOptions(
                    MemberUsage: IsEnabled("--member-usage"), ParameterGroups: IsEnabled("--parameter-groups"),
                    ClassCohesion: IsEnabled("--class-cohesion"), LoopCopy: IsEnabled("--loop-copy"),
                    LoopSearch: IsEnabled("--loop-search"), LoopAllocation: IsEnabled("--loop-allocation"),
                    ResponsibilityConcentration: IsEnabled("--responsibility-concentration")),
                IncludeProjectReferences: IsEnabled("--include-project-references", defaultValue: true),
                MethodMap: IsEnabled("--method-map"));
            var result = Analyzer.Run(options, new ConsoleProgress(), cancellation.Token);
            var report = HtmlReport.Write(result, flags.GetValueOrDefault("--output"));
            Console.WriteLine($"{report} | parsed={result.Parsed}/{result.Sources} | partial={result.ParsedWithErrors} | issues={result.Failures.Count}");
            return result.Failures.Count > 0 || result.Sources == 0 ? 2 : 0;
        }
        catch (OperationCanceledException) { return 130; }
        catch (Exception exception) { Console.Error.WriteLine(exception.Message); return 1; }
    }

    public static void OpenPath(string path) => Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });

    private sealed class ConsoleProgress : IProgress<string>
    {
        public void Report(string value) => Console.Error.WriteLine(value);
    }
}

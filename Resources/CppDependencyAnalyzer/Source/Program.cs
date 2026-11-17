using System.Diagnostics;

namespace ServerDependencyAnalyzer;

internal static class Program
{
    [STAThread]
    private static int Main(string[] arguments)
    {
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
            Console.Error.WriteLine("선택 검사 (기본 false) [--member-usage true|false] [--parameter-groups true|false] [--class-cohesion true|false]");
            Console.Error.WriteLine("                       [--loop-copy true|false] [--loop-search true|false] [--loop-allocation true|false]");
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
                if (flag is not ("--entry" or "--configuration" or "--platform" or "--output" or "--file" or "--include-tests" or "--include-vendor"
                    or "--member-usage" or "--parameter-groups" or "--class-cohesion"
                    or "--loop-copy" or "--loop-search" or "--loop-allocation")
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
                    LoopSearch: IsEnabled("--loop-search"), LoopAllocation: IsEnabled("--loop-allocation")));
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

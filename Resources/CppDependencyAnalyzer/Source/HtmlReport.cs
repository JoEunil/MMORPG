using System.Reflection;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace ServerDependencyAnalyzer;

internal static class HtmlReport
{
    public static string OutputDirectory => Path.Combine(AppContext.BaseDirectory, "Reports");
    public static string LatestReport => Path.Combine(AppContext.BaseDirectory, "report.html");

    public static string Write(AnalysisResult result, string? output)
    {
        if (result.Parsed > 0) VisualStudioLink.AllowRoot(result.Directory);
        var path = output is null
            ? Path.Combine(OutputDirectory, $"{DateTime.Now:yyyyMMdd-HHmmss}-{Guid.NewGuid():N}.html")
            : Path.GetFullPath(output);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var data = JsonSerializer.Serialize(new
        {
            result.Directory, result.Configuration, result.Sources, result.Parsed, result.ParsedWithErrors,
            result.Inspections, result.ElapsedMilliseconds,
            Nodes = result.Nodes.Values.OrderBy(node => node.Kind).ThenBy(node => node.Name).ToArray(),
            Edges = result.Edges.ToArray(), result.Notes, result.Failures, result.ExcludedTests
        }, new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase });
        var html = Render(data);
        File.WriteAllText(path, html, Encoding.UTF8);
        if (output is null)
        {
            File.WriteAllText(LatestReport, html, Encoding.UTF8);
            return LatestReport;
        }
        return path;
    }

    public static void Refresh(string path)
    {
        var previous = File.ReadAllText(path);
        var match = Regex.Match(previous, "<script id=\"data\" type=\"application/json\">([\\s\\S]*?)</script>");
        if (!match.Success) throw new ArgumentException("분석기에서 생성한 보고서가 아닙니다.");
        using var data = JsonDocument.Parse(match.Groups[1].Value);
        File.WriteAllText(path, Render(match.Groups[1].Value), Encoding.UTF8);
    }

    private static string Render(string data) => ReadResource("Report.html")
            .Replace("<!--GRAPH_SCRIPT-->", ReadResource("RelationshipLayout.js") + Environment.NewLine + ReadResource("Report.js"),
                StringComparison.Ordinal)
            .Replace("<!--GRAPH_DATA-->", data, StringComparison.Ordinal);

    private static string ReadResource(string file)
    {
        using var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream($"ServerDependencyAnalyzer.{file}")
            ?? throw new InvalidOperationException($"보고서 리소스를 찾을 수 없습니다. {file}");
        using var reader = new StreamReader(stream, Encoding.UTF8);
        return reader.ReadToEnd();
    }
}

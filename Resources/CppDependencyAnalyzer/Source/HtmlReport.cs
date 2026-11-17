using System.Reflection;
using System.Text;
using System.Text.Json;

namespace ServerDependencyAnalyzer;

internal static class HtmlReport
{
    public static string OutputDirectory => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "ServerDependencyAnalyzer", "Reports");

    public static string Write(AnalysisResult result, string? output)
    {
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
        var html = ReadResource("Report.html")
            .Replace("<!--GRAPH_SCRIPT-->", ReadResource("RelationshipLayout.js") + Environment.NewLine + ReadResource("Report.js"),
                StringComparison.Ordinal)
            .Replace("<!--GRAPH_DATA-->", data, StringComparison.Ordinal);
        File.WriteAllText(path, html, Encoding.UTF8);
        return path;
    }

    private static string ReadResource(string file)
    {
        using var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream($"ServerDependencyAnalyzer.{file}")
            ?? throw new InvalidOperationException($"보고서 리소스를 찾을 수 없습니다. {file}");
        using var reader = new StreamReader(stream, Encoding.UTF8);
        return reader.ReadToEnd();
    }
}

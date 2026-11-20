using System.Reflection;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace ServerDependencyAnalyzer;

internal static class HtmlReport
{
    public static string OutputDirectory => Path.Combine(AppContext.BaseDirectory, "Reports");
    public static string LatestReport => Path.Combine(AppContext.BaseDirectory, "report.html");

    public static string Write(AnalysisResult result, string? output, string? latestAlias = null)
    {
        if (result.Parsed > 0) VisualStudioLink.AllowRoot(result.Directory);
        var path = output is null
            ? Path.Combine(OutputDirectory, $"{DateTime.Now:yyyyMMdd-HHmmss}-{Guid.NewGuid():N}.html")
            : Path.GetFullPath(output);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        latestAlias = output is null ? LatestReport : latestAlias;
        LocalReportService? localService = null;
        if (result.Options is { } options)
        {
            try { localService = ReanalysisService.Register(options, path, latestAlias); }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidOperationException)
            { result.Notes.Add(new(".", "로컬 재분석 서버: " + exception.Message)); }
        }
        var data = JsonSerializer.Serialize(new
        {
            result.Directory, result.Configuration, result.Sources, result.Parsed, result.ParsedWithErrors,
            result.Inspections, result.ElapsedMilliseconds,
            LocalService = localService,
            result.MethodMapEnabled,
            Methods = result.Methods.Values.OrderBy(node => node.Name).ThenBy(node => node.Id).ToArray(),
            MethodCalls = result.MethodCalls.ToArray(),
            Nodes = result.Nodes.Values.OrderBy(node => node.Kind).ThenBy(node => node.Name).ToArray(),
            Edges = result.Edges.ToArray(), result.Notes, result.Failures, result.ExcludedTests
        }, new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase });
        var html = Render(data);
        WriteAtomic(path, html);
        if (latestAlias is not null && !latestAlias.Equals(path, StringComparison.OrdinalIgnoreCase))
            WriteAtomic(latestAlias, html);
        return output is null ? LatestReport : path;
    }

    public static void Refresh(string path)
    {
        var previous = File.ReadAllText(path);
        var match = Regex.Match(previous, "<script id=\"data\" type=\"application/json\">([\\s\\S]*?)</script>");
        if (!match.Success) throw new ArgumentException("분석기에서 생성한 보고서가 아닙니다.");
        using var data = JsonDocument.Parse(match.Groups[1].Value);
        WriteAtomic(path, Render(match.Groups[1].Value));
    }

    private static void WriteAtomic(string path, string html)
    {
        var temporary = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllText(temporary, html, Encoding.UTF8);
            File.Move(temporary, path, overwrite: true);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }

    public static bool IsCurrentReport(string? path, string id)
    {
        if (path is null || !File.Exists(path)) return false;
        return BrowserUrl(path).EndsWith("/reports/" + id, StringComparison.Ordinal);
    }

    public static string BrowserUrl(string path)
    {
        var match = Regex.Match(File.ReadAllText(path), "<script id=\"data\" type=\"application/json\">([\\s\\S]*?)</script>");
        if (!match.Success) return path;
        using var data = JsonDocument.Parse(match.Groups[1].Value);
        if (!data.RootElement.TryGetProperty("localService", out var service) || service.ValueKind != JsonValueKind.Object)
            return path;
        return service.GetProperty("endpoint").GetString() + "/reports/" + service.GetProperty("id").GetString();
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

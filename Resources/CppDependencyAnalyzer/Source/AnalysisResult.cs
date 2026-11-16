namespace ServerDependencyAnalyzer;

internal record SourceLocation(string File, uint Line, uint Column = 0);
internal record Evidence(SourceLocation Location, string Description);
internal record StudioFilter(string Project, string Name);
internal record Node(string Id, string Name, string Kind, string? Project, SourceLocation? Location,
    string? Namespace = null, StudioFilter? VisualStudioFilter = null, bool IsStruct = false, bool IsAbstract = false);
internal record AnalysisIssue(string File, string Message);

internal sealed class Edge(string from, string to, string kind)
{
    public string From { get; } = from;
    public string To { get; } = to;
    public string Kind { get; } = kind;
    public List<Evidence> Evidence { get; } = [];
}

internal sealed class AnalysisResult(string directory, string configuration)
{
    private readonly Dictionary<(string From, string To, string Kind), Edge> edges = [];

    public string Directory { get; } = directory;
    public string Configuration { get; } = configuration;
    public Dictionary<string, Node> Nodes { get; } = new(StringComparer.Ordinal);
    public Dictionary<string, List<StudioFilter>> StudioFilters { get; } = new(StringComparer.OrdinalIgnoreCase);
    public IEnumerable<Edge> Edges => edges.Values.OrderBy(edge => edge.Kind)
        .ThenBy(edge => edge.From).ThenBy(edge => edge.To);
    public List<AnalysisIssue> Notes { get; } = [];
    public List<AnalysisIssue> Failures { get; } = [];
    public SortedSet<string> ExcludedTests { get; } = new(StringComparer.OrdinalIgnoreCase);
    public int Sources { get; set; }
    public int Parsed { get; set; }
    public int ParsedWithErrors { get; set; }

    public void Connect(string from, string to, string kind, Evidence evidence)
    {
        if (from == to)
            return;

        var key = (from, to, kind);
        if (!edges.TryGetValue(key, out var edge))
        {
            edge = new Edge(from, to, kind);
            edges.Add(key, edge);
        }

        if (edge.Evidence.Count < 12 && !edge.Evidence.Contains(evidence))
            edge.Evidence.Add(evidence);
    }
}

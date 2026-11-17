using System.Diagnostics;
using System.Runtime.InteropServices;
using Cursor = ServerDependencyAnalyzer.ClangNative.Cursor;
using static ServerDependencyAnalyzer.AstGraph;
using static ServerDependencyAnalyzer.ClangNative;

namespace ServerDependencyAnalyzer;

internal sealed class CodeInspections(InspectionOptions options, CancellationToken cancellation)
{
    private readonly Dictionary<string, ClassInfo> classes = new(StringComparer.Ordinal);
    private readonly Dictionary<(string Source, string Target), Dictionary<string, Evidence>> memberUses = [];
    private readonly Dictionary<string, FunctionSignature> signatures = new(StringComparer.Ordinal);
    private readonly Dictionary<string, Dictionary<SourceLocation, LoopFindings>> loopCandidates = [];
    private FunctionScope? function;
    private LoopScope? loop;
    private RangeVariable? range;
    private long collectionTicks;

    internal readonly record struct Scope(FunctionScope? Function, LoopScope? Loop, RangeVariable? Range, bool InBody);
    internal sealed class FunctionScope(Cursor cursor, string usr, SourceLocation location, bool definition,
        string? owner, MethodInfo? method, List<Parameter>? parameters)
    {
        public Cursor Cursor { get; } = cursor;
        public string Usr { get; } = usr;
        public SourceLocation Location { get; } = location;
        public bool Definition { get; } = definition;
        public string? Owner { get; } = owner;
        public MethodInfo? Method { get; } = method;
        public List<Parameter>? Parameters { get; } = parameters;
        public bool InBody { get; set; }
    }

    internal sealed class LoopScope(Cursor cursor, SourceLocation location, LoopScope? parent)
    {
        public Cursor Cursor { get; } = cursor;
        public SourceLocation Location { get; } = location;
        public int Depth { get; } = (parent?.Depth ?? 0) + 1;
        public bool InBody { get; private set; }
        public List<LoopHit> Candidates { get; } = [];
        private int children;

        public void BeginChild()
        {
            // libclang visits a do-loop body first and the other loop bodies last.
            InBody = Cursor.Kind != Kind.DoStmt || children == 0;
            if (InBody)
                Candidates.Clear();
            children++;
        }
    }

    internal record LoopHit(string Rule, SourceLocation Loop, int Depth, Evidence Site);
    private sealed class LoopFindings(SourceLocation location, int depth)
    {
        public SourceLocation Location { get; } = location;
        public int Depth { get; } = depth;
        public Dictionary<Evidence, Evidence> Sites { get; } = [];
    }

    internal sealed class RangeVariable(Cursor cursor, SourceLocation location, string type, string record)
    {
        public Cursor Cursor { get; } = cursor;
        public SourceLocation Location { get; } = location;
        public string Type { get; } = type;
        public string Record { get; } = record;
        public bool Checked { get; set; }
    }

    internal sealed class MethodInfo(string name)
    {
        public string Name { get; } = name;
        public SourceLocation? Body { get; set; }
        public Dictionary<string, Evidence> Fields { get; } = new(StringComparer.Ordinal);
        public HashSet<string> Calls { get; } = new(StringComparer.Ordinal);
        public Dictionary<string, Evidence> Dependencies { get; } = new(StringComparer.Ordinal);
    }

    private sealed class ClassInfo(string name, SourceLocation location)
    {
        public string Name { get; } = name;
        public SourceLocation Location { get; } = location;
        public HashSet<string> PublicMethods { get; } = new(StringComparer.Ordinal);
        public Dictionary<string, MethodInfo> Methods { get; } = new(StringComparer.Ordinal);
    }

    internal readonly record struct Parameter(string Name, string Type);
    private sealed record FunctionSignature(Parameter[] Parameters, Evidence Function, int Named, bool Definition);

    public Scope Enter(Cursor cursor, Cursor parent, SourceLocation source)
    {
        var started = Stopwatch.GetTimestamp();
        var previous = new Scope(function, loop, range, function?.InBody == true);
        SkipChild(parent);
        var kind = cursor.Kind;
        if (IsClass(kind) || IsFunction(kind) || kind == Kind.LambdaExpr)
        {
            function = null;
            loop = null;
            range = null;
            if (IsClass(kind))
                ReadClass(cursor, source);
            else if (kind == Kind.LambdaExpr)
                function = new(cursor, "", source, true, null, null, null);
            else
                function = ReadFunction(cursor, source);
        }
        else if (function is { } scope)
        {
            if (parent == scope.Cursor)
            {
                if (kind == Kind.ParmDecl && scope.Parameters is { } parameters)
                    parameters.Add(new(Name(cursor), Read(clang_getTypeSpelling(clang_getCursorType(cursor)))));

                if (kind is Kind.CompoundStmt or Kind.CxxTryStmt)
                {
                    scope.InBody = true;
                    if (scope.Method is { } method)
                        method.Body ??= scope.Location;
                }
                else if (options.MemberUsage && scope.Definition && scope.Cursor.Kind == Kind.Constructor &&
                    (uint)kind is >= 100 and < 200)
                    scope.InBody = true;
            }

            if (scope.InBody)
            {
                if (scope.Owner is not null && (options.MemberUsage || options.ClassCohesion))
                    ReadMemberUse(cursor, source, scope);
                if (options.LoopCopy || options.LoopSearch || options.LoopAllocation)
                    ReadLoop(cursor, parent, source);
            }
        }
        collectionTicks += Stopwatch.GetTimestamp() - started;
        return previous;
    }

    public void SkipChild(Cursor parent)
    {
        if (loop is { } current && parent == current.Cursor)
            current.BeginChild();
    }

    public void EndUnit()
    {
        function = null;
        loop = null;
        range = null;
    }

    public void Exit(Cursor cursor, Scope previous)
    {
        var started = Stopwatch.GetTimestamp();
        if (function is { Parameters: { Count: >= 3 } } scope && cursor == scope.Cursor)
            SaveSignature(scope);

        if (loop is { } current && cursor == current.Cursor)
        {
            foreach (var candidate in current.Candidates)
            {
                if (previous.Loop is null)
                    SaveLoopCandidate(candidate);
                else if (previous.Loop.InBody)
                    previous.Loop.Candidates.Add(candidate);
            }
        }

        function = previous.Function;
        if (function is not null)
            function.InBody = previous.InBody;
        loop = previous.Loop;
        range = previous.Range;
        collectionTicks += Stopwatch.GetTimestamp() - started;
    }

    private void ReadClass(Cursor cursor, SourceLocation source)
    {
        if (!options.MemberUsage && !options.ClassCohesion)
            return;
        var usr = Usr(cursor);
        if (usr.Length != 0 && !classes.ContainsKey(usr))
            classes.Add(usr, new(QualifiedName(cursor), source));
    }

    private FunctionScope ReadFunction(Cursor cursor, SourceLocation source)
    {
        var definition = clang_isCursorDefinition(cursor) != 0;
        var owner = options.MemberUsage || options.ClassCohesion ? ClassUsr(clang_getCursorSemanticParent(cursor)) : null;
        var usr = options.ParameterGroups || owner is not null ? Usr(cursor) : "";
        MethodInfo? method = null;
        if (usr.Length > 0 && owner is not null && classes.TryGetValue(owner, out var info) && IsMethod(cursor))
        {
            if (options.MemberUsage && clang_getCXXAccessSpecifier(cursor) == 1)
                info.PublicMethods.Add(usr);
            if (options.ClassCohesion && !info.Methods.TryGetValue(usr, out method))
                info.Methods.Add(usr, method = new(Read(clang_getCursorDisplayName(cursor))));
        }
        return new(cursor, usr, source, definition, owner, method,
            options.ParameterGroups && usr.Length != 0 ? [] : null);
    }

    private void ReadMemberUse(Cursor cursor, SourceLocation source, FunctionScope scope)
    {
        if (cursor.Kind is not (Kind.CallExpr or Kind.MemberRefExpr or Kind.DeclRefExpr))
            return;
        var declaration = clang_getCursorReferenced(cursor);
        var field = cursor.Kind != Kind.CallExpr && declaration.Kind is Kind.FieldDecl or Kind.VarDecl;
        var method = IsMethod(declaration);
        var call = cursor.Kind == Kind.CallExpr && IsFunction(declaration.Kind);
        if (!field && !method && (scope.Method is null || !call))
            return;

        var parent = clang_getCursorSemanticParent(declaration);
        var owner = ClassUsr(parent);
        if (owner is not null && (field || method))
        {
            var usr = Usr(declaration);
            if (usr.Length == 0)
                return;
            if (owner == scope.Owner && scope.Method is { } own)
            {
                if (field)
                    own.Fields.TryAdd(usr, new(source, Name(declaration)));
                else if (call)
                    own.Calls.Add(usr);
            }
            else if (owner != scope.Owner && options.MemberUsage && classes.ContainsKey(owner))
            {
                var pair = (scope.Owner!, owner);
                if (!memberUses.TryGetValue(pair, out var members))
                    memberUses.Add(pair, members = new(StringComparer.Ordinal));
                members.TryAdd(usr, new(source, method ? Read(clang_getCursorDisplayName(declaration)) : Name(declaration)));
            }
        }

        if (scope.Method is { } current && owner != scope.Owner && call)
        {
            var dependency = owner is null ? QualifiedName(declaration) : QualifiedName(parent);
            if (dependency.Length != 0)
                current.Dependencies.TryAdd(dependency, new(source, dependency));
        }
    }

    private void ReadLoop(Cursor cursor, Cursor parent, SourceLocation source)
    {
        var kind = cursor.Kind;
        if (kind is Kind.ForStmt or Kind.WhileStmt or Kind.DoStmt or Kind.CxxForRangeStmt)
        {
            loop = new(cursor, source, loop);
            range = null;
            return;
        }
        if (loop is not { } enclosing)
            return;

        if (options.LoopCopy && enclosing.Cursor.Kind == Kind.CxxForRangeStmt &&
            kind == Kind.VarDecl && parent == enclosing.Cursor)
        {
            var type = clang_getCanonicalType(clang_getCursorType(cursor));
            var declaration = clang_getTypeDeclaration(type);
            var name = Name(declaration);
            if (name is "vector" or "basic_string" && InStdNamespace(clang_getCursorSemanticParent(declaration)))
                range = new(cursor, source, Read(clang_getTypeSpelling(type)), name);
            return;
        }

        if (kind == Kind.CallExpr && range is { Checked: false } variable)
        {
            var constructor = clang_getCursorReferenced(cursor);
            if (constructor.Kind == Kind.Constructor && clang_getCursorSemanticParent(constructor) is var type &&
                Name(type) == variable.Record && InStdNamespace(clang_getCursorSemanticParent(type)))
            {
                variable.Checked = true;
                if (clang_CXXConstructor_isCopyConstructor(constructor) != 0)
                    SaveLoopCandidate(InLoop("loop-copy", variable.Location, enclosing,
                        $"{Name(variable.Cursor)} ({variable.Type})의 복사 생성자가 각 반복에서 호출됩니다."));
            }
        }
        if (!enclosing.InBody)
            return;

        if (options.LoopAllocation && kind == Kind.CxxNewExpr && IsOrdinaryNew(cursor))
            enclosing.Candidates.Add(InLoop("loop-allocation", source, enclosing, "new 표현식"));
        if (kind != Kind.CallExpr || (!options.LoopSearch && !options.LoopAllocation))
            return;

        var target = clang_getCursorReferenced(cursor);
        if (target.Kind is not (Kind.FunctionDecl or Kind.FunctionTemplate))
            return;
        var nameOfTarget = Name(target);
        var targetParent = clang_getCursorSemanticParent(target);
        if (options.LoopSearch && (nameOfTarget is "find" or "find_if" or "find_if_not") && InStdNamespace(targetParent))
            enclosing.Candidates.Add(InLoop("loop-search", source, enclosing, $"std::{nameOfTarget} 호출"));
        if (options.LoopAllocation && IsAllocationFunction(target, nameOfTarget, targetParent))
            enclosing.Candidates.Add(InLoop("loop-allocation", source, enclosing, $"{QualifiedName(target)} 호출"));
    }

    private static bool IsAllocationFunction(Cursor function, string name, Cursor parent) =>
        name is "make_unique" or "make_shared" or "make_unique_for_overwrite" or "make_shared_for_overwrite"
            ? InStdNamespace(parent)
            : (name is "malloc" or "calloc" or "realloc" or "aligned_alloc") &&
              (SkipLinkage(parent).Kind == Kind.TranslationUnit || InStdNamespace(parent)) &&
              clang_Location_isInSystemHeader(clang_getCursorLocation(function)) != 0;

    private static bool IsOrdinaryNew(Cursor cursor)
    {
        // The C interface provides no placement-argument flag. An opening parenthesis after
        // new is ambiguous with a parenthesized type, so report only the unambiguous form.
        var unit = clang_Cursor_getTranslationUnit(cursor);
        clang_tokenize(unit, clang_getCursorExtent(cursor), out var tokens, out var count);
        try
        {
            var sawNew = false;
            for (uint index = 0; index < count; index++)
            {
                var token = Marshal.PtrToStructure<Token>(tokens + (int)index * Marshal.SizeOf<Token>());
                if (clang_getTokenKind(token) == 4)
                    continue;
                var spelling = Read(clang_getTokenSpelling(unit, token));
                if (sawNew)
                    return spelling != "(";
                if (spelling == "new")
                    sawNew = true;
                else if (spelling != "::")
                    return false;
            }
            return false;
        }
        finally { clang_disposeTokens(unit, tokens, count); }
    }

    private static bool InStdNamespace(Cursor cursor)
    {
        cursor = SkipLinkage(cursor);
        while (cursor.Kind == Kind.Namespace && clang_Cursor_isInlineNamespace(cursor) != 0)
            cursor = SkipLinkage(clang_getCursorSemanticParent(cursor));
        return cursor.Kind == Kind.Namespace && Name(cursor) == "std" &&
            SkipLinkage(clang_getCursorSemanticParent(cursor)).Kind == Kind.TranslationUnit;
    }

    private static Cursor SkipLinkage(Cursor cursor)
    {
        while (cursor.Kind == Kind.LinkageSpec)
            cursor = clang_getCursorSemanticParent(cursor);
        return cursor;
    }

    private static bool IsMethod(Cursor cursor) =>
        cursor.Kind is Kind.CxxMethod or Kind.ConversionFunction ||
        cursor.Kind == Kind.FunctionTemplate && clang_getTemplateCursorKind(cursor) is Kind.CxxMethod or Kind.ConversionFunction;

    private static string Usr(Cursor cursor) => Read(clang_getCursorUSR(cursor));
    private static string? ClassUsr(Cursor cursor) => IsClass(cursor.Kind) && Usr(cursor) is { Length: > 0 } id ? id : null;

    private static LoopHit InLoop(string rule, SourceLocation source, LoopScope loop, string detail) =>
        new(rule, loop.Location, loop.Depth, new(source, detail));

    private void SaveLoopCandidate(LoopHit hit)
    {
        if (!loopCandidates.TryGetValue(hit.Rule, out var candidates))
            loopCandidates.Add(hit.Rule, candidates = []);
        var key = LocationKey(hit.Loop);
        if (!candidates.TryGetValue(key, out var findings))
            candidates.Add(key, findings = new(hit.Loop, hit.Depth));
        findings.Sites.TryAdd(hit.Site with { Location = LocationKey(hit.Site.Location) }, hit.Site);
    }

    // Clang can report a Windows header with different casing across translation units.
    private static SourceLocation LocationKey(SourceLocation location) => location with { File = location.File.ToUpperInvariant() };

    private void SaveSignature(FunctionScope scope)
    {
        var parameters = scope.Parameters!;
        var named = parameters.Count(parameter => parameter.Name.Length != 0 && parameter.Type.Length != 0);
        if (named < 3 || signatures.TryGetValue(scope.Usr, out var prior) &&
            (prior.Named > named || prior.Named == named && (prior.Definition || !scope.Definition)))
            return;
        signatures[scope.Usr] = new(parameters.ToArray(), new(scope.Location, QualifiedName(scope.Cursor)), named, scope.Definition);
    }

    public InspectionReport Finish()
    {
        var started = Stopwatch.GetTimestamp();
        var report = InspectionReport.Create(options, rule => Ordered(rule switch
        {
            "member-usage" => MemberCandidates(),
            "parameter-groups" => ParameterCandidates(),
            "class-cohesion" => CohesionCandidates(),
            _ => LoopCandidates(rule)
        }));
        return report with
        {
            CollectionMilliseconds = (collectionTicks + Stopwatch.GetTimestamp() - started) * 1000.0 / Stopwatch.Frequency
        };
    }

    private IEnumerable<InspectionCandidate> LoopCandidates(string rule)
    {
        if (!loopCandidates.TryGetValue(rule, out var candidates))
            yield break;
        var title = rule switch
        {
            "loop-copy" => "범위 반복문에서 값 복사",
            "loop-search" => "반복문 본문에서 선형 검색",
            _ => "반복문 본문에서 동적 할당"
        };
        foreach (var findings in candidates.Values)
        {
            cancellation.ThrowIfCancellationRequested();
            yield return new(title, $"중첩 깊이 {findings.Depth} · 관찰 위치 {findings.Sites.Count}곳", findings.Location,
                findings.Sites.Values.OrderBy(site => site.Location.File, StringComparer.OrdinalIgnoreCase)
                    .ThenBy(site => site.Location.Line).ThenBy(site => site.Location.Column).ToArray());
        }
    }

    private IEnumerable<InspectionCandidate> MemberCandidates()
    {
        foreach (var (pair, uses) in memberUses)
        {
            cancellation.ThrowIfCancellationRequested();
            if (uses.Count > 2 || !classes.TryGetValue(pair.Source, out var source) ||
                !classes.TryGetValue(pair.Target, out var target) || target.PublicMethods.Count < 5)
                continue;
            var members = uses.Values.OrderBy(member => member.Description, StringComparer.Ordinal).ToArray();
            yield return new($"{source.Name} → {target.Name}",
                $"참조한 멤버 {uses.Count}개 ({string.Join(", ", members.Select(member => member.Description))}) · 관찰된 대상 공개 메서드 {target.PublicMethods.Count}개",
                members[0].Location, [.. members, new(target.Location, target.Name)]);
        }
    }

    private IEnumerable<InspectionCandidate> ParameterCandidates()
    {
        var groups = new Dictionary<string, (Parameter[] Parameters, List<string> Functions)>(StringComparer.Ordinal);
        foreach (var (id, signature) in signatures.OrderBy(pair => pair.Key, StringComparer.Ordinal))
        {
            cancellation.ThrowIfCancellationRequested();
            var parameters = signature.Parameters;
            var seen = new HashSet<string>(StringComparer.Ordinal);
            for (var start = 0; start <= parameters.Length - 3; start++)
            {
                var key = "";
                for (var end = start; end < parameters.Length; end++)
                {
                    var parameter = parameters[end];
                    if (parameter.Name.Length == 0 || parameter.Type.Length == 0)
                        break;
                    key += parameter.Name + '\u001f' + parameter.Type + '\u001e';
                    if (end - start < 2 || !seen.Add(key))
                        continue;
                    if (!groups.TryGetValue(key, out var group))
                        groups.Add(key, group = (parameters[start..(end + 1)], []));
                    group.Functions.Add(id);
                }
            }
        }

        foreach (var sameFunctions in groups.Values.Where(group => group.Functions.Count >= 3)
            .GroupBy(group => string.Join('\u001f', group.Functions)))
        {
            cancellation.ThrowIfCancellationRequested();
            var retained = new List<Parameter[]>();
            foreach (var group in sameFunctions.OrderByDescending(group => group.Parameters.Length))
            {
                if (retained.Any(longer => ContainsGroup(longer, group.Parameters)))
                    continue;
                retained.Add(group.Parameters);
                var locations = group.Functions.Select(id => signatures[id].Function).ToArray();
                yield return new("여러 함수에서 반복되는 연속 매개변수",
                    $"{string.Join(", ", group.Parameters.Select(parameter => $"{parameter.Type} {parameter.Name}"))} · 함수 {locations.Length}개",
                    locations[0].Location, locations);
            }
        }
    }

    private static bool ContainsGroup(Parameter[] longer, Parameter[] group)
    {
        for (var start = 0; start <= longer.Length - group.Length; start++)
            if (longer.AsSpan(start, group.Length).SequenceEqual(group))
                return true;
        return false;
    }

    private IEnumerable<InspectionCandidate> CohesionCandidates()
    {
        foreach (var info in classes.Values)
        {
            cancellation.ThrowIfCancellationRequested();
            var methods = info.Methods;
            if (methods.Count < 4 || methods.Values.Any(method => method.Body is null))
                continue;
            var links = methods.Keys.ToDictionary(id => id, _ => new HashSet<string>(), StringComparer.Ordinal);
            var fieldOwners = new Dictionary<string, string>(StringComparer.Ordinal);
            void Link(string a, string b)
            {
                links[a].Add(b);
                links[b].Add(a);
            }
            foreach (var (id, method) in methods)
            {
                foreach (var field in method.Fields.Keys)
                {
                    if (!fieldOwners.TryAdd(field, id))
                        Link(id, fieldOwners[field]);
                }
                foreach (var target in method.Calls)
                    if (links.ContainsKey(target))
                        Link(id, target);
            }

            var groups = new List<List<MethodInfo>>();
            var seen = new HashSet<string>(StringComparer.Ordinal);
            foreach (var id in links.Keys)
            {
                if (!seen.Add(id))
                    continue;
                var pending = new Stack<string>();
                pending.Push(id);
                var group = new List<MethodInfo>();
                while (pending.TryPop(out var next))
                {
                    group.Add(methods[next]);
                    foreach (var target in links[next])
                        if (seen.Add(target))
                            pending.Push(target);
                }
                if (group.Count >= 2)
                    groups.Add(group);
            }

            if (groups.Count < 2)
                continue;
            var evidence = new List<Evidence>();
            for (var index = 0; index < groups.Count; index++)
            {
                var group = groups[index];
                evidence.AddRange(group.OrderBy(method => method.Name, StringComparer.Ordinal)
                    .Select(method => new Evidence(method.Body!, $"그룹 {index + 1} · {method.Name}")));
                evidence.AddRange(group.SelectMany(method => method.Fields.Values)
                    .DistinctBy(field => field.Description).Select(field => field with { Description = $"그룹 {index + 1} 필드 · {field.Description}" }));
                evidence.AddRange(group.SelectMany(method => method.Dependencies.Values)
                    .DistinctBy(dependency => dependency.Description).Take(8)
                    .Select(dependency => dependency with { Description = $"그룹 {index + 1} 외부 의존 · {dependency.Description}" }));
            }
            yield return new(info.Name, $"자체 필드와 내부 호출을 기준으로 서로 연결되지 않은 메서드 그룹 {groups.Count}개가 관찰됐습니다.",
                info.Location, evidence);
        }
    }

    private static InspectionCandidate[] Ordered(IEnumerable<InspectionCandidate> candidates) =>
        candidates.OrderBy(candidate => candidate.Location.File, StringComparer.OrdinalIgnoreCase)
            .ThenBy(candidate => candidate.Location.Line).ThenBy(candidate => candidate.Location.Column)
            .ThenBy(candidate => candidate.Title, StringComparer.Ordinal).ToArray();
}

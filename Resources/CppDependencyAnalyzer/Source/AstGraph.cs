using System.Runtime.ExceptionServices;
using Cursor = ServerDependencyAnalyzer.ClangNative.Cursor;
using static ServerDependencyAnalyzer.ClangNative;

namespace ServerDependencyAnalyzer;

internal sealed class AstGraph(string root, AnalysisResult result, bool includeTests, bool showLibraries, CancellationToken cancellation)
{
    private readonly (string Directory, string Project)[] projectDirectories = result.Nodes.Values
        .Where(node => node.Kind == "project" && node.Location is not null)
        .Select(node => (Directory: Path.GetDirectoryName(Path.Combine(root, node.Location!.File))!, Project: node.Id))
        .OrderByDescending(item => item.Directory.Length).ToArray();
    private readonly Dictionary<nint, string?> files = [];
    private readonly Dictionary<Cursor, string?> classIds = [];
    private ExceptionDispatchInfo? visitorFailure;
    private string? currentClass;
    private string? currentFunction;
    private string? currentMethod;
    private bool collectMethods;
    private readonly HashSet<string> definedMethods = new(StringComparer.Ordinal);
    private string? unitProject;
    private string? unitFile;
    private CodeInspections? inspections;

    public void ReadUnit(nint unit, CompileInput input, CodeInspections? collector, bool methodMap = false)
    {
        unitProject = input.Project;
        unitFile = Path.GetRelativePath(root, input.File);
        files.Clear();
        classIds.Clear();
        currentClass = null;
        currentFunction = null;
        currentMethod = null;
        collectMethods = methodMap;
        visitorFailure = null;
        inspections = collector;
        try
        {
            clang_visitChildren(clang_getTranslationUnitCursor(unit), VisitCursor, 0);
            visitorFailure?.Throw();
            cancellation.ThrowIfCancellationRequested();
        }
        finally
        {
            classIds.Clear();
            inspections?.EndUnit();
            inspections = null;
        }
    }

    private Visit VisitCursor(Cursor cursor, Cursor parent, nint data)
    {
        if (cancellation.IsCancellationRequested || visitorFailure is not null)
            return Visit.Break;

        try
        {
            ReadCursor(cursor, parent);
            return cancellation.IsCancellationRequested || visitorFailure is not null ? Visit.Break : Visit.Continue;
        }
        catch (Exception exception)
        {
            visitorFailure = ExceptionDispatchInfo.Capture(exception);
            return Visit.Break;
        }
    }

    private void ReadCursor(Cursor cursor, Cursor parent)
    {
        var kind = cursor.Kind;
        if (kind is Kind.MacroDefinition or Kind.MacroExpansion)
            return;

        var source = Site(clang_getCursorLocation(cursor));
        if (source is null)
        {
            inspections?.SkipChild(parent);
            return;
        }

        if (kind == Kind.InclusionDirective)
        {
            if (showLibraries)
                ReadLibraryInclude(cursor, source);
            return;
        }

        var previousClass = currentClass;
        var previousFunction = currentFunction;
        var previousMethod = currentMethod;
        if (IsClass(kind))
        {
            if (clang_isCursorDefinition(cursor) == 0)
                return;
            currentClass = ClassNode(cursor);
            currentMethod = null;
        }
        else if (IsFunction(kind))
        {
            currentClass = ClassNode(clang_getCursorSemanticParent(cursor));
            if (currentClass is not null && (kind is Kind.CxxMethod or Kind.Destructor or Kind.ConversionFunction)
                && result.Nodes[currentClass] is { IsAbstract: false } node
                && clang_CXXMethod_isPureVirtual(cursor) != 0)
                result.Nodes[currentClass] = node with { IsAbstract = true };
            currentFunction = QualifiedName(cursor);
            currentMethod = collectMethods ? MethodNode(cursor) : null;
        }
        else if (currentClass is not null && kind is Kind.FieldDecl or Kind.CxxBaseSpecifier)
        {
            var type = clang_getCursorType(cursor);
            var relation = kind == Kind.FieldDecl ? "field" : "inherits";
            var label = kind == Kind.FieldDecl ? $"{Name(cursor)} · {Read(clang_getTypeSpelling(type))}"
                : Read(clang_getTypeSpelling(type));
            if (kind == Kind.CxxBaseSpecifier && inspections is { TracksResponsibility: true })
                inspections.RecordBase(currentClass, ClassNode(clang_getTypeDeclaration(type)) ??
                    ClassNode(clang_getTypeDeclaration(clang_getCanonicalType(type))));
            foreach (var target in ClassesInType(type, 0))
            {
                var evidence = new Evidence(source, label);
                result.Connect(currentClass, target, relation, evidence);
                if (kind == Kind.FieldDecl && inspections is { TracksResponsibility: true })
                    inspections.RecordDependency(currentClass, target, evidence);
            }
        }
        else if (kind == Kind.CallExpr)
        {
            var declaration = clang_getCursorReferenced(cursor);
            if (clang_Cursor_isNull(declaration) == 0)
            {
                var target = ClassNode(clang_getCursorSemanticParent(declaration));
                if (currentClass is not null && target is not null && IsFunction(declaration.Kind))
                {
                    var evidence = new Evidence(source, $"{currentFunction} → {QualifiedName(declaration)}");
                    result.Connect(currentClass, target, "call", evidence);
                    if (collectMethods && currentMethod is not null && MethodNode(declaration) is { } targetMethod)
                        result.ConnectMethods(currentMethod, targetMethod, new(source,
                            $"{result.Methods[currentMethod].Name} → {result.Methods[targetMethod].Name}"));
                    if (inspections is { TracksResponsibility: true })
                        inspections.RecordDependency(currentClass, target, evidence);
                }
            }
        }

        if (inspections is null)
            clang_visitChildren(cursor, VisitCursor, 0);
        else
        {
            var scope = inspections.Enter(cursor, parent, source);
            try { clang_visitChildren(cursor, VisitCursor, 0); }
            finally { inspections.Exit(cursor, scope); }
        }
        currentClass = previousClass;
        currentFunction = previousFunction;
        currentMethod = previousMethod;
    }

    private string? MethodNode(Cursor original)
    {
        if (!IsFunction(original.Kind))
            return null;
        var cursor = original;
        var template = clang_getSpecializedCursorTemplate(cursor);
        if (IsFunction(template.Kind))
            cursor = template;
        var definition = clang_getCursorDefinition(cursor);
        if (IsFunction(definition.Kind) && Site(clang_getCursorLocation(definition)) is not null)
            cursor = definition;
        var owner = ClassNode(clang_getCursorSemanticParent(cursor));
        var location = Site(clang_getCursorLocation(cursor));
        if (owner is null || location is null)
            return null;
        var usr = Read(clang_getCursorUSR(cursor));
        if (usr.Length == 0)
            return null;
        var id = "method:" + usr;
        var isDefinition = clang_isCursorDefinition(cursor) != 0;
        if (!result.Methods.ContainsKey(id) || (isDefinition && !definedMethods.Contains(id)))
        {
            var ownerNode = result.Nodes[owner];
            var signature = Read(clang_getCursorDisplayName(cursor));
            var type = Read(clang_getTypeSpelling(clang_getCursorType(cursor)));
            // Display names omit member cv/ref qualifiers, which can distinguish overloads.
            var closing = type.LastIndexOf(')');
            if (closing >= 0 && closing + 1 < type.Length)
                signature += type[(closing + 1)..];
            result.Methods[id] = new(id, ownerNode.Name + "::" + signature, "method", ownerNode.Project,
                location, ownerNode.Name, ownerNode.VisualStudioFilter, ClassId: owner);
            if (isDefinition)
                definedMethods.Add(id);
        }
        return id;
    }

    private IEnumerable<string> ClassesInType(CXType type, int depth)
    {
        if (type.Kind == 0 || depth == 7)
            yield break;
        var id = ClassNode(clang_getTypeDeclaration(type));
        if (id is not null)
            yield return id;

        var pointee = clang_getPointeeType(type);
        if (pointee.Kind == 0)
            pointee = clang_getArrayElementType(type);
        foreach (var target in ClassesInType(pointee, depth + 1))
            yield return target;

        var count = Math.Min(16, clang_Type_getNumTemplateArguments(type));
        for (var item = 0; item < count; item++)
            foreach (var target in ClassesInType(clang_Type_getTemplateArgumentAsType(type, (uint)item), depth + 1))
                yield return target;

        if (id is null && pointee.Kind == 0 && count < 1)
        {
            var canonical = clang_getCanonicalType(type);
            if (canonical != type)
                foreach (var target in ClassesInType(canonical, depth + 1))
                    yield return target;
        }
    }

    private string? ClassNode(Cursor original)
    {
        if (!IsClass(original.Kind))
            return null;
        if (classIds.TryGetValue(original, out var existing))
            return existing;

        var cursor = original;
        var template = clang_getSpecializedCursorTemplate(cursor);
        if (IsClass(template.Kind))
            cursor = template;
        var definition = clang_getCursorDefinition(cursor);
        if (IsClass(definition.Kind))
            cursor = definition;
        var location = Site(clang_getCursorLocation(cursor));
        var usr = location is null ? "" : Read(clang_getCursorUSR(cursor));
        string? id = null;
        if (location is not null && usr.Length > 0)
        {
            id = "class:" + usr;
            var isAbstract = IsClass(definition.Kind) && clang_CXXRecord_isAbstract(definition) != 0;
            if (!result.Nodes.TryGetValue(id, out var node))
            {
                var project = ProjectFor(location.File);
                var filters = result.StudioFilters.GetValueOrDefault(location.File);
                var filter = filters?.FirstOrDefault(entry => entry.Project == project) ?? filters?.FirstOrDefault();
                var declarationKind = cursor.Kind is Kind.ClassTemplate or Kind.ClassTemplatePartialSpecialization
                    ? clang_getTemplateCursorKind(cursor)
                    : cursor.Kind;
                result.Nodes.Add(id, new(id, QualifiedName(cursor), "class", project, location, NamespaceName(cursor),
                    filter, IsStruct: declarationKind == Kind.StructDecl, IsAbstract: isAbstract));
            }
            else if (isAbstract && !node.IsAbstract)
                result.Nodes[id] = node with { IsAbstract = true };
        }
        classIds[original] = id;
        return id;
    }

    private SourceLocation? Site(Location location)
    {
        clang_getExpansionLocation(location, out var file, out var line, out var column, out _);
        if (file == 0)
            return null;
        if (!files.TryGetValue(file, out var relative))
        {
            relative = null;
            if (clang_Location_isInSystemHeader(location) == 0)
            {
                var name = Read(clang_getFileName(file));
                if (Path.IsPathFullyQualified(name) && ProjectFiles.Inside(root, name))
                {
                    var path = Path.GetRelativePath(root, name);
                    if (!includeTests && ProjectFiles.IsTestPath(path))
                        result.ExcludedTests.Add(path);
                    else if (!ProjectFiles.IsBenchmarkPath(path) && ProjectFiles.ExternalPackage(path) is null)
                        relative = path;
                }
            }
            files[file] = relative;
        }
        return relative is null ? null : new(relative, line, column);
    }

    private void ReadLibraryInclude(Cursor cursor, SourceLocation source)
    {
        var included = clang_getIncludedFile(cursor);
        if (included == 0)
            return;
        var path = Read(clang_getFileName(included));
        if (!Path.IsPathFullyQualified(path) || !ProjectFiles.Inside(root, path))
            return;

        var relative = Path.GetRelativePath(root, path);
        var package = ProjectFiles.ExternalPackage(relative);
        if (package is null)
            return;

        var project = string.Equals(source.File, unitFile, StringComparison.OrdinalIgnoreCase) ? unitProject : null;
        project ??= result.StudioFilters.GetValueOrDefault(source.File)?.FirstOrDefault()?.Project ?? ProjectFor(source.File);
        if (project is null)
            return;

        var library = package.Value;
        var id = "library:" + library.Name.ToUpperInvariant();
        result.Nodes.TryAdd(id, new(id, library.Name, "library", null, new(library.Path, 0)));
        result.Connect(project, id, "external", new(source, $"#include {relative}"));
    }

    private string NamespaceName(Cursor cursor)
    {
        var parts = new Stack<string>();
        for (var parent = clang_getCursorSemanticParent(cursor);
            clang_Cursor_isNull(parent) == 0 && parent.Kind != Kind.TranslationUnit;
            parent = clang_getCursorSemanticParent(parent))
        {
            if (parent.Kind != Kind.Namespace)
                continue;
            var name = Name(parent);
            parts.Push(name.Length == 0
                ? $"(익명 · {Site(clang_getCursorLocation(parent))?.File ?? "소스 미확인"})"
                : name);
        }
        return string.Join("::", parts);
    }

    private string? ProjectFor(string file)
    {
        var absolute = Path.Combine(root, file);
        return projectDirectories.FirstOrDefault(item => ProjectFiles.Inside(item.Directory, absolute)).Project;
    }

    internal static string Name(Cursor cursor) => Read(clang_getCursorSpelling(cursor));

    internal static string QualifiedName(Cursor cursor)
    {
        var names = new Stack<string>();
        while (clang_Cursor_isNull(cursor) == 0 && cursor.Kind != Kind.TranslationUnit)
        {
            var name = Name(cursor);
            if (!string.IsNullOrEmpty(name))
                names.Push(name);
            cursor = clang_getCursorSemanticParent(cursor);
        }
        return string.Join("::", names);
    }

    internal static bool IsClass(Kind kind) => kind is Kind.ClassDecl or Kind.StructDecl or Kind.UnionDecl
        or Kind.ClassTemplate or Kind.ClassTemplatePartialSpecialization;
    internal static bool IsFunction(Kind kind) => kind is Kind.FunctionDecl or Kind.FunctionTemplate or Kind.CxxMethod
        or Kind.Constructor or Kind.Destructor or Kind.ConversionFunction;
}

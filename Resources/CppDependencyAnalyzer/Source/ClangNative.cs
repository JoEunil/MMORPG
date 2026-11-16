using System.Reflection;
using System.Runtime.InteropServices;

namespace ServerDependencyAnalyzer;

internal static class ClangNative
{
    private const string Library = "libclang";
    private static nint libraryHandle;

    static ClangNative() => NativeLibrary.SetDllImportResolver(typeof(ClangNative).Assembly, Resolve);

    private static nint Resolve(string name, Assembly assembly, DllImportSearchPath? searchPath) =>
        name == Library ? libraryHandle : nint.Zero;

    public static void Load(string file)
    {
        if (libraryHandle == nint.Zero)
            libraryHandle = NativeLibrary.Load(file);
    }

    [StructLayout(LayoutKind.Sequential)]
    internal readonly record struct Cursor(Kind Kind, int DataIndex, nint Data1, nint Data2, nint Data3);
    [StructLayout(LayoutKind.Sequential)]
    internal readonly record struct CXType(int Kind, nint Data1, nint Data2);
    [StructLayout(LayoutKind.Sequential)]
    internal readonly record struct CXString(nint Data, uint PrivateFlags);
    [StructLayout(LayoutKind.Sequential)]
    internal readonly record struct Location(nint Data1, nint Data2, uint Index);
    // Values from clang-c/Index.h, the stable libclang C interface.
    internal enum Kind : uint
    {
        StructDecl = 2, UnionDecl = 3, ClassDecl = 4, FieldDecl = 6, FunctionDecl = 8,
        CxxMethod = 21, Namespace = 22, Constructor = 24,
        Destructor = 25, ConversionFunction = 26, FunctionTemplate = 30, ClassTemplate = 31,
        ClassTemplatePartialSpecialization = 32, CxxBaseSpecifier = 44,
        CallExpr = 103, TranslationUnit = 350, MacroDefinition = 501, MacroExpansion = 502, InclusionDirective = 503
    }

    internal enum Visit : uint { Break, Continue, Recurse }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate Visit Visitor(Cursor cursor, Cursor parent, nint data);

    internal static string Read(CXString value)
    {
        try { return Marshal.PtrToStringUTF8(clang_getCString(value)) ?? ""; }
        finally { clang_disposeString(value); }
    }

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern nint clang_createIndex(int excludeDeclarationsFromPch, int displayDiagnostics);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void clang_disposeIndex(nint index);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int clang_parseTranslationUnit2(nint index, [MarshalAs(UnmanagedType.LPUTF8Str)] string source, [In] nint[] arguments, int count, nint unsaved, uint unsavedCount, uint options, out nint unit);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void clang_disposeTranslationUnit(nint unit);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint clang_getNumDiagnostics(nint unit);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern nint clang_getDiagnostic(nint unit, uint index);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint clang_getDiagnosticSeverity(nint diagnostic);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint clang_defaultDiagnosticDisplayOptions();

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern CXString clang_formatDiagnostic(nint diagnostic, uint options);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void clang_disposeDiagnostic(nint diagnostic);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Cursor clang_getTranslationUnitCursor(nint unit);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint clang_visitChildren(Cursor parent, Visitor visitor, nint data);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint clang_isCursorDefinition(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint clang_Cursor_isNull(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern CXString clang_getCursorSpelling(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern CXString clang_getCursorUSR(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Cursor clang_getCursorReferenced(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Cursor clang_getCursorSemanticParent(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Cursor clang_getCursorDefinition(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint clang_CXXRecord_isAbstract(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint clang_CXXMethod_isPureVirtual(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Cursor clang_getSpecializedCursorTemplate(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Kind clang_getTemplateCursorKind(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern CXType clang_getCursorType(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern CXString clang_getTypeSpelling(CXType type);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Cursor clang_getTypeDeclaration(CXType type);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern CXType clang_getCanonicalType(CXType type);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern CXType clang_getPointeeType(CXType type);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern CXType clang_getArrayElementType(CXType type);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int clang_Type_getNumTemplateArguments(CXType type);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern CXType clang_Type_getTemplateArgumentAsType(CXType type, uint index);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Location clang_getCursorLocation(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void clang_getExpansionLocation(Location location, out nint file, out uint line, out uint column, out uint offset);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int clang_Location_isInSystemHeader(Location location);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern CXString clang_getFileName(nint file);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern nint clang_getIncludedFile(Cursor cursor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern nint clang_getCString(CXString value);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void clang_disposeString(CXString value);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern CXString clang_getClangVersion();
}

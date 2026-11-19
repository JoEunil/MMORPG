using System.Diagnostics;
using System.Text.Json;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using Microsoft.Win32;

namespace ServerDependencyAnalyzer;

internal static class VisualStudioLink
{
    private const string Scheme = "cppdeps";
    private static string RootsFile => Path.Combine(AppContext.BaseDirectory, "SourceRoots.json");

    public static void Register(string? root = null)
    {
        _ = FindVisualStudio();
        var executable = Path.Combine(AppContext.BaseDirectory, "ServerDependencyAnalyzer.exe");
        if (!File.Exists(executable)) throw new FileNotFoundException("분석기 실행파일을 찾지 못했습니다.", executable);
        if (!string.IsNullOrEmpty(root)) AllowRoot(root);
        using var key = Registry.CurrentUser.CreateSubKey(@"Software\Classes\" + Scheme);
        key.SetValue("", "URL:C++ dependency analyzer source link");
        key.SetValue("URL Protocol", "");
        using var command = key.CreateSubKey(@"shell\open\command");
        command.SetValue("", $"\"{executable}\" --open-source \"%1\"");
    }

    public static void AllowRoot(string root)
    {
        root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));
        if (!Directory.Exists(root)) throw new DirectoryNotFoundException(root);
        var roots = ReadRoots();
        if (roots.Contains(root, StringComparer.OrdinalIgnoreCase)) return;
        File.WriteAllText(RootsFile, JsonSerializer.Serialize(roots.Append(root).ToArray()));
    }

    private static string[] ReadRoots() => File.Exists(RootsFile)
        ? JsonSerializer.Deserialize<string[]>(File.ReadAllText(RootsFile)) ?? [] : [];

    private sealed record SourceTarget(string File, int Line, string? Workspace);
    private enum OpenResult { NotFound, Busy, Opened }

    public static void Open(string link)
    {
        var target = Resolve(link);
        if (target.Workspace is null) { Process.Start(BuildStartInfo(target)); return; }
        // /Edit may open the solution as a document in another instance. Match the loaded solution instead.
        var until = Environment.TickCount64 + 30_000;
        var started = false;
        do
        {
            var result = TryOpenRunning(target);
            if (result == OpenResult.Opened) return;
            if (result == OpenResult.NotFound && !started)
            {
                Process.Start(BuildStartInfo(target));
                started = true;
            }
            Thread.Sleep(300);
        } while (Environment.TickCount64 < until);
        if (!started) throw new InvalidOperationException("Visual Studio가 사용 중입니다. 잠시 후 링크를 다시 선택하세요.");
        // The startup File.OpenFile command still opens the file if automation isn't available.
    }

    public static ProcessStartInfo CreateStartInfo(string link) => BuildStartInfo(Resolve(link));

    private static SourceTarget Resolve(string link)
    {
        if (!Uri.TryCreate(link, UriKind.Absolute, out var uri) || uri.Scheme != Scheme || uri.Host != "open"
            || uri.AbsolutePath is not ("" or "/") || uri.UserInfo.Length > 0 || !uri.IsDefaultPort || uri.Fragment.Length > 0)
            throw new ArgumentException("지원하지 않는 소스 링크입니다.");
        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var part in uri.Query.TrimStart('?').Split('&'))
        {
            var pair = part.Split('=', 2);
            if (pair.Length != 2 || pair[0] is not ("root" or "file" or "line")
                || !values.TryAdd(pair[0], Uri.UnescapeDataString(pair[1])))
                throw new ArgumentException("소스 링크의 인자가 잘못됐습니다.");
        }
        if (!values.TryGetValue("root", out var root) || !Path.IsPathFullyQualified(root)
            || !values.TryGetValue("file", out var relative) || Path.IsPathRooted(relative)
            || !values.TryGetValue("line", out var lineText) || !int.TryParse(lineText, out var line) || line < 1)
            throw new ArgumentException("소스 경로와 행 번호를 확인하세요.");
        root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));
        if (!ReadRoots().Contains(root, StringComparer.OrdinalIgnoreCase))
            throw new ArgumentException("분석기에 등록된 소스 폴더가 아닙니다. 해당 폴더를 선택해 VS 연결을 등록하세요.");
        if (relative.Split(['/', '\\']).Any(part => part.Length == 0 || part is "." or ".."
            || part.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0))
            throw new ArgumentException("소스 폴더 밖의 파일은 열 수 없습니다.");
        var file = Path.GetFullPath(relative, root);
        var extensions = new[] { ".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh", ".hxx", ".inl", ".ixx", ".cppm",
            ".vcxproj", ".filters", ".props", ".sln", ".slnx" };
        var prefix = Path.EndsInDirectorySeparator(root) ? root : root + Path.DirectorySeparatorChar;
        if (!file.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)
            || !extensions.Contains(Path.GetExtension(file), StringComparer.OrdinalIgnoreCase) || !File.Exists(file))
            throw new ArgumentException("열 수 있는 C/C++ 소스 또는 프로젝트 파일이 아닙니다.");
        for (var part = file; !part.Equals(root, StringComparison.OrdinalIgnoreCase); part = Path.GetDirectoryName(part)!)
            if ((File.GetAttributes(part) & FileAttributes.ReparsePoint) != 0)
                throw new ArgumentException("다른 경로로 연결된 파일이나 폴더는 열 수 없습니다.");

        var workspace = FindWorkspace(root, file, ["*.sln", "*.slnx"])
            ?? FindWorkspace(root, file, ["*.vcxproj"]);
        return new(file, line, workspace);
    }

    private static ProcessStartInfo BuildStartInfo(SourceTarget target)
    {
        var start = new ProcessStartInfo(FindVisualStudio()) { UseShellExecute = false };
        if (target.Workspace is not null)
        {
            start.ArgumentList.Add(target.Workspace);
            start.ArgumentList.Add("/Command");
            start.ArgumentList.Add($"File.OpenFile \"{target.File}\"");
        }
        else
        {
            start.ArgumentList.Add("/Edit");
            start.ArgumentList.Add(target.File);
            start.ArgumentList.Add("/Command");
            start.ArgumentList.Add("Edit.GoTo " + target.Line);
        }
        return start;
    }

    [DllImport("ole32.dll")]
    private static extern int GetRunningObjectTable(uint reserved, out IRunningObjectTable table);
    [DllImport("ole32.dll")]
    private static extern int CreateBindCtx(uint reserved, out IBindCtx context);

    private static OpenResult TryOpenRunning(SourceTarget target)
    {
        var busy = false;
        IRunningObjectTable? table = null;
        IBindCtx? context = null;
        IEnumMoniker? enumerator = null;
        try
        {
            Marshal.ThrowExceptionForHR(GetRunningObjectTable(0, out table));
            Marshal.ThrowExceptionForHR(CreateBindCtx(0, out context));
            table.EnumRunning(out enumerator);
            var monikers = new IMoniker[1];
            while (enumerator.Next(1, monikers, IntPtr.Zero) == 0)
            {
                object? instance = null;
                try
                {
                    monikers[0].GetDisplayName(context, null, out var name);
                    if (!name.StartsWith("!VisualStudio.DTE.", StringComparison.Ordinal)) continue;
                    table.GetObject(monikers[0], out instance);
                    dynamic dte = instance;
                    var solution = (string)dte.Solution.FullName;
                    if (string.IsNullOrEmpty(solution) || target.Workspace is null
                        || !Path.GetFullPath(solution).Equals(target.Workspace, StringComparison.OrdinalIgnoreCase)) continue;
                    dynamic document = dte.ActiveDocument;
                    if (document is null || !target.File.Equals((string)document.FullName, StringComparison.OrdinalIgnoreCase))
                    {
                        dte.ItemOperations.OpenFile(target.File);
                        return OpenResult.Busy;
                    }
                    document.Selection.GotoLine(target.Line, false);
                    dte.MainWindow.Activate();
                    return OpenResult.Opened;
                }
                catch (Exception exception) when (exception is COMException or MissingMemberException)
                { busy = true; /* Opening a document and reading its selection may complete on different UI turns. */ }
                finally
                {
                    if (instance is not null && Marshal.IsComObject(instance)) Marshal.ReleaseComObject(instance);
                    Marshal.ReleaseComObject(monikers[0]);
                }
            }
        }
        catch (COMException) { busy = true; }
        finally
        {
            if (enumerator is not null) Marshal.ReleaseComObject(enumerator);
            if (context is not null) Marshal.ReleaseComObject(context);
            if (table is not null) Marshal.ReleaseComObject(table);
        }
        return busy ? OpenResult.Busy : OpenResult.NotFound;
    }

    private static string? FindWorkspace(string root, string file, string[] patterns)
    {
        for (var directory = Path.GetDirectoryName(file); directory is not null; directory = Path.GetDirectoryName(directory))
        {
            var choices = patterns.SelectMany(pattern => Directory.EnumerateFiles(directory, pattern)).ToArray();
            if (choices.Length == 1) return choices[0];
            if (choices.Length > 1 || directory.Equals(root, StringComparison.OrdinalIgnoreCase)) return null;
        }
        return null;
    }

    private static string FindVisualStudio()
    {
        var vswhere = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            "Microsoft Visual Studio", "Installer", "vswhere.exe");
        if (!File.Exists(vswhere)) throw new FileNotFoundException("Visual Studio Installer의 vswhere.exe를 찾지 못했습니다.");
        var start = new ProcessStartInfo(vswhere) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true };
        foreach (var argument in new[] { "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.CoreEditor", "-property", "productPath" })
            start.ArgumentList.Add(argument);
        using var process = Process.Start(start) ?? throw new InvalidOperationException("Visual Studio를 찾지 못했습니다.");
        var executable = process.StandardOutput.ReadToEnd().Trim();
        process.WaitForExit();
        if (process.ExitCode != 0 || !File.Exists(executable)) throw new FileNotFoundException("Visual Studio IDE를 찾지 못했습니다.");
        return executable;
    }
}

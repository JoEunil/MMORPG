using System.Diagnostics;

namespace ServerDependencyAnalyzer;

internal sealed record ClangInstallation(string Library, string? ResourceDirectory, string? MsvcVersion,
    string[] SystemIncludes)
{
    public static ClangInstallation Find()
    {
        var programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
        var programFilesX86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
        var visualStudio = FindVisualStudio(programFilesX86);
        var configured = Environment.GetEnvironmentVariable("LIBCLANG_PATH");
        var candidates = new List<string>();
        if (!string.IsNullOrWhiteSpace(configured))
            candidates.Add(Directory.Exists(configured) ? Path.Combine(configured, "libclang.dll") : configured);
        candidates.Add(Path.Combine(AppContext.BaseDirectory, "clang", "libclang.dll"));
        candidates.Add(Path.Combine(programFiles, "LLVM", "bin", "libclang.dll"));
        if (visualStudio is not null)
        {
            candidates.Add(Path.Combine(visualStudio, "VC", "Tools", "Llvm", "x64", "bin", "libclang.dll"));
            candidates.Add(Path.Combine(visualStudio, "VC", "Tools", "Llvm", "bin", "libclang.dll"));
        }

        var library = candidates.FirstOrDefault(File.Exists);
        if (library is null)
            throw new InvalidOperationException("libclang.dll을 찾을 수 없습니다. Get-ClangRuntime.ps1로 준비하거나 LIBCLANG_PATH를 지정하세요.");

        var binaryDirectory = Path.GetDirectoryName(Path.GetFullPath(library))!;
        var resource = Environment.GetEnvironmentVariable("CLANG_RESOURCE_DIR");
        if (string.IsNullOrWhiteSpace(resource))
        {
            var installedResources = Path.GetFullPath(Path.Combine(binaryDirectory, "..", "lib", "clang"));
            var portableResources = Path.Combine(binaryDirectory, "resources");
            resource = Directory.Exists(Path.Combine(portableResources, "include")) ? portableResources : LatestVersion(installedResources);
        }

        var includeDirectories = new List<string>();
        var msvc = visualStudio is null ? null : LatestVersion(Path.Combine(visualStudio, "VC", "Tools", "MSVC"));
        var sdk = LatestVersion(Path.Combine(programFilesX86, "Windows Kits", "10", "Include"));
        if (msvc is not null)
        {
            includeDirectories.Add(Path.Combine(msvc, "include"));
            includeDirectories.Add(Path.Combine(msvc, "atlmfc", "include"));
        }
        if (sdk is not null)
            includeDirectories.AddRange(new[] { "ucrt", "shared", "um", "winrt", "cppwinrt" }.Select(name => Path.Combine(sdk, name)));
        includeDirectories.AddRange((Environment.GetEnvironmentVariable("INCLUDE") ?? "")
            .Split(';', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries));

        var version = msvc is null ? null : Version.Parse(Path.GetFileName(msvc));
        return new(Path.GetFullPath(library), resource, version is null ? null : $"19.{version.Minor}",
            includeDirectories.Where(Directory.Exists).Distinct(StringComparer.OrdinalIgnoreCase).ToArray());
    }

    private static string? LatestVersion(string directory)
    {
        if (!Directory.Exists(directory))
            return null;
        return Directory.EnumerateDirectories(directory)
            .Select(path =>
            {
                var match = Path.GetFileName(path);
                return (Path: path, Version: Version.TryParse(match, out var version) ? version :
                    int.TryParse(match, out var major) ? new Version(major, 0) : null);
            }).Where(item => item.Version is not null)
            .OrderByDescending(item => item.Version).Select(item => item.Path).FirstOrDefault();
    }

    private static string? FindVisualStudio(string baseDirectory)
    {
        var finder = Path.Combine(baseDirectory, "Microsoft Visual Studio", "Installer", "vswhere.exe");
        if (!File.Exists(finder))
            return null;
        var options = new ProcessStartInfo(finder)
        {
            UseShellExecute = false,
            RedirectStandardOutput = true,
            CreateNoWindow = true
        };
        foreach (var argument in new[] { "-latest", "-products", "*", "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath" })
            options.ArgumentList.Add(argument);
        using var process = Process.Start(options)!;
        var output = process.StandardOutput.ReadToEndAsync();
        if (!process.WaitForExit(5000))
        {
            process.Kill();
            return null;
        }
        var path = output.GetAwaiter().GetResult().Trim();
        return Directory.Exists(path) ? path : null;
    }
}

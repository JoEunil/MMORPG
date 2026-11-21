using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEngine;

namespace Mmorpg.Client.Editor
{
    public sealed class SharedGameDataBuild : IPreprocessBuildWithReport, IPostprocessBuildWithReport
    {
        public int callbackOrder => 0;

        [MenuItem("Tools/Client/Content/Reload Shared CSV")]
        public static void Reload()
        {
            var content = AssetDatabase.LoadAssetAtPath<ServerContent>("Assets/Client/Content/ServerContent.asset");
            if (content == null) throw new InvalidOperationException("The client content asset is missing.");
            content.LoadCsv(SharedGameData.DirectoryPath);
            EditorUtility.SetDirty(content);
            AssetDatabase.SaveAssets();
        }

        public void OnPreprocessBuild(BuildReport report)
        {
            if (report.summary.platform != BuildTarget.StandaloneWindows64 &&
                report.summary.platform != BuildTarget.StandaloneLinux64 &&
                report.summary.platform != BuildTarget.StandaloneOSX)
                throw new BuildFailedException("Shared CSV deployment currently supports desktop players.");
            Reload();
        }

        public void OnPostprocessBuild(BuildReport report)
        {
            string output = report.summary.outputPath;
            string directory = report.summary.platform == BuildTarget.StandaloneOSX
                ? Path.Combine(output, "Contents")
                : Path.GetDirectoryName(output);
            string target = Path.Combine(directory, "Data");
            foreach (string source in Directory.GetFiles(SharedGameData.DirectoryPath, "*.csv", SearchOption.AllDirectories))
            {
                string relative = source.Substring(SharedGameData.DirectoryPath.Length + 1);
                string destination = Path.Combine(target, relative);
                Directory.CreateDirectory(Path.GetDirectoryName(destination));
                File.Copy(source, destination, true);
            }
        }
    }
}

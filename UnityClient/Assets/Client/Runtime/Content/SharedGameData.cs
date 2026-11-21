using System.IO;
using UnityEngine;

namespace Mmorpg.Client
{
    public static class SharedGameData
    {
        public static string DirectoryPath => Path.GetFullPath(Path.Combine(
            Application.dataPath, Application.isEditor ? "../.." : Application.platform == RuntimePlatform.OSXPlayer ? "." : "..", "Data"));
    }
}

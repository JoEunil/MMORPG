#if UNITY_EDITOR
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;

namespace Mmorpg.Client.Tests
{
    // Runs the existing C01 assertions while allowing the Editor's native input pump to tick.
    public static class LoginEditorInputRun
    {
        private static readonly Stack<IEnumerator> steps = new Stack<IEnumerator>();
        private static double nextStep;
        private static double deadline;
        private static string resultPath;
        public static bool Running => steps.Count > 0;

        public static void Start()
        {
            var method = typeof(LoginFlowTests).GetMethod(
                "RealUiInput_FailedLoginRetry_AndSceneReload_DoesNotDuplicateListEvents");
            Start((IEnumerator)method.Invoke(new LoginFlowTests(), null),
                Path.Combine(Application.dataPath, "../Captures/c01-editor-input.txt"),
                "PASS: unchanged C01 UI assertions completed.");
        }

        public static void Start(IEnumerator scenario, string outputPath, string passMessage)
        {
            if (!EditorApplication.isPlaying || Running || scenario == null)
                throw new InvalidOperationException("Editor input run requires Play mode and an idle scenario.");
            steps.Push(scenario);
            resultPath = Path.GetFullPath(outputPath);
            Directory.CreateDirectory(Path.GetDirectoryName(resultPath));
            File.WriteAllText(resultPath, "Editor-update UI execution: RUNNING\nNot a NUnitRunner result.\n");
            completionMessage = passMessage;
            deadline = EditorApplication.timeSinceStartup + 120;
            nextStep = 0;
            EditorApplication.update += Tick;
            EditorApplication.playModeStateChanged += OnPlayModeChange;
        }

        private static string completionMessage;

        private static void Tick()
        {
            if (EditorApplication.timeSinceStartup < nextStep) return;
            nextStep = EditorApplication.timeSinceStartup + 0.05;
            if (EditorApplication.timeSinceStartup > deadline)
            {
                Finish("FAIL: execution deadline exceeded.");
                return;
            }
            try
            {
                var current = steps.Peek();
                if (!current.MoveNext())
                {
                    steps.Pop();
                    (current as IDisposable)?.Dispose();
                    if (steps.Count == 0) Finish(completionMessage);
                }
                else if (current.Current is IEnumerator nested)
                    steps.Push(nested);
                else if (current.Current != null)
                    Finish("FAIL: unsupported yield type.");
            }
            catch (Exception exception)
            {
                // No exception message/stack: a future assertion must not copy secrets into this file.
                Finish("FAIL: " + exception.GetType().Name + "; step=" + steps.Peek().GetType().Name);
            }
        }

        private static void OnPlayModeChange(PlayModeStateChange state)
        {
            if (state == PlayModeStateChange.ExitingPlayMode)
                Finish("FAIL: Play mode ended before completion.");
        }

        private static void Finish(string result)
        {
            EditorApplication.update -= Tick;
            EditorApplication.playModeStateChanged -= OnPlayModeChange;
            while (steps.Count > 0)
            {
                try { (steps.Pop() as IDisposable)?.Dispose(); }
                catch { result = "FAIL: iterator cleanup failed."; }
            }
            File.AppendAllText(resultPath, result + "\nFinished UTC: " + DateTime.UtcNow.ToString("O") + "\n");
            completionMessage = null;
        }
    }
}
#endif

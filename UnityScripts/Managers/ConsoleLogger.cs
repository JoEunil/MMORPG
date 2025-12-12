using System;
using UnityEngine;
using ClientCore;
public class ConsoleLogger : MonoBehaviour, ClientCore.ILogger
{
    [SerializeField] private MainThreadDispatcher dispatcher;

    private void Awake()
    {
        DontDestroyOnLoad(gameObject); // 씬 전환 시 유지하고 싶으면
    }
    public void Log(string message)
    {
        string timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
        string logMessage = $"[{timestamp}] {message}" + Environment.NewLine;
        if (dispatcher != null)
        {
            dispatcher.Post(() => Debug.Log(logMessage));
        }
        else
        { 
            Debug.Log("MainThreadDispatcher not set to ConsoleLogger");
        }
    }
}
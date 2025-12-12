using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using ClientCore;

namespace Client
{

    public class UILogger : ILogger
    {
        public static UILogger Instance { get; } = new UILogger();
        public event Action<string> LogToUI;
        public void Log(string message)
        {
            string timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
            string logMessage = $"[{timestamp}] {message}" + Environment.NewLine;
            LogToUI?.Invoke(logMessage);
        }
    }
}

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Threading;
using static ClientCore.Config;

namespace ClientCore
{
    internal class ClientTick
    {
        private Thread _thread;
        private int _running;
        internal void Start()
        {
            _running = 1;
            _thread = new Thread(Work);
            _thread.IsBackground = true; // 메인 종료 시 같이 종료
            _thread.Start();
        }

        internal void Stop()
        {
            Interlocked.Exchange(ref _running, 0);
            _thread?.Join(); // 스레드 종료 대기
            _thread = null;
        }

        private void Work()
        {
            Stopwatch sw = new Stopwatch();

            while (_running != 0)
            {
                sw.Restart();
                Program.Action();
                int sleepTime = GAME_TICK - (int)sw.ElapsedMilliseconds;
                if (sleepTime > 0)
                {
                    Thread.Sleep(sleepTime); 
                }
            }
        }
    }
}

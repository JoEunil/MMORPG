using ClientCore.PacketHelper;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Threading;
using static ClientCore.Config;

namespace ClientCore.Network
{
    internal class ReceiveTick :IReceiveTick
    {
        private readonly IHandlerTick _handler;
        private Thread _thread;
        private int _running;
        ConcurrentQueue<byte[]> _recvQ ; // 스레드 안전
        public ReceiveTick(IHandlerTick handler)
        {
            _handler = handler;
            _recvQ = new ConcurrentQueue<byte[]>();
        }
        public void Start()
        {
            _running = 1;
            _thread = new Thread(Work);
            _thread.IsBackground = true; // 메인 종료 시 같이 종료
            _thread.Start();
        }

        public void Stop()
        {
            Interlocked.Exchange(ref _running, 0);
            _thread?.Join(); // 스레드 종료 대기
            _thread = null;
        }

        private void Work()
        {
            var localQueue = new Queue<byte[]>();
            Stopwatch sw = new Stopwatch();

            while (_running != 0)
            {
                
                var tickStart = sw.ElapsedMilliseconds;
                while (_recvQ.TryDequeue(out var packet))
                    localQueue.Enqueue(packet);
                while (localQueue.Count > 0)
                {
                    var curr = localQueue.Dequeue();
                    var header = Packet.ExtractHeader(curr);

                    switch ((OP_CODE)header.opcode)
                    {
                        case OP_CODE.ZONE_CHANGE_RESPONSE:
                            _handler.HandleZoneChangeRes(Packet.Deserialize<ZoneChangeResponseBody>(header, curr));
                            break;
                        case OP_CODE.ZONE_DELTA_UPDATE_BROADCAST:
                            _handler.HandleDeltaUpdate(Packet.DeserializeVariablePacket<DeltaUpdateField>(header, curr));
                            break;
                        case OP_CODE.ZONE_FULL_STATE_BROADCAST:
                            _handler.HandleFullState(Packet.DeserializeVariablePacket<FullStateField>(header, curr));
                            break;
                        case OP_CODE.MONSTER_DELTA_UPDATE_BROADCAST:
                            _handler.HandleMonsterDelta(Packet.DeserializeVariablePacket<MonsterDeltaField>(header, curr));
                            break;
                        case OP_CODE.MONSTER_FULL_STATE_BROADCAST:
                            _handler.HandleMonsterFull(Packet.DeserializeVariablePacket<MonsterFullField>(header, curr));
                            break;
                        case OP_CODE.ACTION_RESULT:
                            _handler.HandleActionReult(Packet.DeserializeVariablePacket<ActionResultField>(header, curr));
                            break;
                        }
                }
                int sleepTime = GAME_TICK - (int)(sw.ElapsedMilliseconds - tickStart);
                if (sleepTime > 0)
                {
                    Thread.Sleep(sleepTime);
                }
            }
        }
        public void Enqueue(byte[] buffer)
        {
            _recvQ.Enqueue(buffer);
        }
    }
}

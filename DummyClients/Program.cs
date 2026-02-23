using ClientCore;
using ClientCore.Network;
using ClientCore.Services;
using System;
using System.Collections.Generic;
using System.Diagnostics.Metrics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using static System.Collections.Specialized.BitVector32;
static class Program
{
    static private List<ClientSession> sessions = new List<ClientSession>();
    static private ViewModel _viewModel;
    static private NetworkService _network;
    static private Handler _handler;
    static private ClientTick _clientTick;
    static private int _ready = 0;
    static private int _count = 0;
    static private ulong tick = 0;
    static private byte dir = 0;
    static public TCPSocket GetSocket(int index)
    {
        return sessions[index].GetSocket();
    }
    static public void Initialize(int count)
    {
        _count = count;
        _network = new NetworkService();
        _viewModel = new ViewModel(_network);
        _handler = new Handler();
        _clientTick = new ClientTick();
        _handler.Initialize(_viewModel);
        TCPSocket.Initialize(_handler);
        for (int i = 0; i < count; i++)
        {
            sessions.Add(new ClientSession(new TCPSocket(i)));
        }
        Console.WriteLine("Session initialized " + count);
    }
    static public async Task Connect()
    {
        var i = 1;
        foreach (var session in sessions)
        {
            var id = "test" + i;
            var pwd = "12345";
            await AuthService.Instance.LoginAsync(session, id, pwd);
            (var address, var port) = await AuthService.Instance.GetSessionAsync(session);
            await _network.Connect(session, address, port);
            i++;
            Thread.Sleep(10);
        }
        Console.WriteLine("connect ");
    }
    static public void Ready()
    {
        _ready++;
    }
    static public void StartAction()
    {
        while (true)
        {
            Console.WriteLine(_ready);
            if (_ready == _count)
                break;
            Thread.Sleep(1000);
        }
    }
    static public void Action()
    {
        tick++;
        if ((tick & 15) == 15)
        {
            dir++;
            dir &= 3;
        }
        bool chat = (tick & 31) == 31;
        foreach (var session in sessions.ToList())
        {
            if(!_network.Action(session.GetSocket(), (byte)dir, 1, 255))
            {
                sessions.Remove(session);
                continue;
            }
            if (chat)
            {
                if (!_network.Chat(session.GetSocket(), "test" + session.UserID, 1, 1))
                {
                    sessions.Remove(session);
                    continue;
                }
            }
        }
        _count = sessions.Count();
    }
    static async Task Main(string[] args)
    {
        int clientCount = 100;   // 원하는 더미 클라이언트 수

        Initialize(clientCount);
        await Connect();

        // 모든 세션이 준비될 때까지 기다림
        StartAction();

        // 이후 자동 이동/채팅 등 Action 시작
        Console.WriteLine("Dummy clients running...");
        
        _clientTick.Start();
        while (true)
        {
            Console.WriteLine("current client: " + _count);
            Thread.Sleep(10000);
        }
    }
}
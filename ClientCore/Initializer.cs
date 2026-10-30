using ClientCore.Network;
using ClientCore.Services;
using System;
using System.Collections.Generic;
using System.Text;

namespace ClientCore
{
    public class Initializer : IDisposable
    {
        private ViewModel _viewModel;
        private ViewData _data;
        private ClientTick _clientTick;
        private NetworkService _network;
        private TCPSocket _sock;
        private Handler _handler;
        public void Initialize(ILogger logger, IMainThreadDispatcher dispatcher)
        {
            if (_viewModel != null) throw new InvalidOperationException("Already initialized.");
            _handler = new Handler();
            _data = new ViewData();
            _sock = new TCPSocket(_handler, logger);
            _network = new NetworkService(_sock, logger);
            _viewModel = new ViewModel(_network, _data, logger, dispatcher);
            _sock.OnDisconnected += _viewModel.Error;
            _clientTick = new ClientTick(_viewModel);
            _handler.Inject(_viewModel);
            _clientTick.Start();
        }

        public void Dispose()
        {
            _viewModel?.Dispose();
            if (_sock != null && _viewModel != null) _sock.OnDisconnected -= _viewModel.Error;
            _sock?.Dispose();
            _clientTick?.Stop();
        }

        public IViewModelUI GetViewModel()
        {
            return _viewModel;
        }
        public IViewDataUI GetViewData()
        {
            return _data;
        }
    }
}

using ClientCore.Network;
using ClientCore.Services;
using System;
using System.Collections.Generic;
using System.Text;

namespace ClientCore
{
    public class Initializer
    {
        private ViewModel _viewModel;
        private ViewData _data;
        private ClientTick _clientTick;
        private NetworkService _network;
        private TCPSocket _sock;
        private ReceiveTick _receiveTick;
        private Handler _handler;
        public void Initialize(ILogger logger, IMainThreadDispatcher dispatcher)
        {
            _handler = new Handler();
            _data = new ViewData();
            _sock = new TCPSocket(_handler, logger);
            _network = new NetworkService(_sock, logger);
            _viewModel = new ViewModel(_network, _data, logger, dispatcher);
            _clientTick = new ClientTick(_viewModel);
            _receiveTick = new ReceiveTick(_handler);
            _handler.Inject(_viewModel, _receiveTick);
            _clientTick.Start();
            _receiveTick.Start();
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

using UnityEngine;
using ClientCore;

public class CoreManager : MonoBehaviour
{
    public static CoreManager Instance { get; private set; }
    public ConsoleLogger _logger;
    [SerializeField] private MainThreadDispatcher _dispatcher;
    ClientCore.Initializer _core;
    public IViewModelUI VM { get; private set; }
    public IViewDataUI VD { get; private set; }

    void Awake()
    {
        if (Instance != null)
        {
            Destroy(gameObject);
            return;
        }
        Instance = this;
        DontDestroyOnLoad(gameObject);

        _core = new ClientCore.Initializer();
        _core.Initialize(_logger, _dispatcher);
        VM = _core.GetViewModel();
        VD = _core.GetViewData();
    }

}

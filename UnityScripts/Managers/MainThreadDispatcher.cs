using System.Collections.Concurrent;
using UnityEngine;
using System;
public class MainThreadDispatcher : MonoBehaviour, ClientCore.IMainThreadDispatcher
{
    private readonly ConcurrentQueue<Action> jobs = new ConcurrentQueue<Action>();

    public void Post(Action job)
    {
        jobs.Enqueue(job);
    }

    private void Start()
    {
        DontDestroyOnLoad(this);
    }
    private void Update()
    {
        while (jobs.TryDequeue(out var job))
            job?.Invoke();
    }
}
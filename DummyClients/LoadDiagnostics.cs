using System.Collections.Concurrent;
using System.Diagnostics;
using System.Text;

namespace ClientCore
{
    internal static class LoadDiagnostics
    {
        private static readonly object StartLock = new();
        private static Run? _active;
        private static bool _started;

        internal static IDisposable Start(int requestedClients)
        {
            lock (StartLock)
            {
                if (_started) return EmptyLease.Instance;
                _started = true;
                try
                {
                    var run = new Run(requestedClients);
                    Volatile.Write(ref _active, run);
                    run.Start();
                    Console.WriteLine($"Diagnostics: {run.FilePath}");
                    return run;
                }
                catch (Exception error)
                {
                    Volatile.Write(ref _active, null);
                    Console.Error.WriteLine($"Diagnostics disabled: {error.Message}");
                    return EmptyLease.Instance;
                }
            }
        }

        internal static void SetPhase(string phase)
        {
            var run = Volatile.Read(ref _active);
            if (run != null) Volatile.Write(ref run.Phase, phase);
        }

        internal static void SetSessionCounts(int slots, int ready)
        {
            var run = Volatile.Read(ref _active);
            if (run == null) return;
            Volatile.Write(ref run.Slots, slots);
            Volatile.Write(ref run.Ready, ready);
        }

        internal static void RecordPing()
        {
            var run = Volatile.Read(ref _active);
            if (run != null) Interlocked.Increment(ref run.Pings);
        }

        internal static void RecordPongCompleted(long receivedTimestamp)
        {
            var run = Volatile.Read(ref _active);
            if (run == null) return;
            run.PongLatency.Record(receivedTimestamp);
            Interlocked.Increment(ref run.PongsCompleted);
        }

        internal static void RecordPongFailed()
        {
            var run = Volatile.Read(ref _active);
            if (run != null) Interlocked.Increment(ref run.PongsFailed);
        }

        internal static void RecordSendQueued(bool pong)
        {
            var run = Volatile.Read(ref _active);
            if (run == null) return;
            Interlocked.Increment(ref run.SendWaiters);
            if (pong) Interlocked.Increment(ref run.PongWaiters);
        }

        internal static void RecordSendWait(long startedTimestamp, bool pong)
        {
            var run = Volatile.Read(ref _active);
            if (run == null) return;
            Interlocked.Decrement(ref run.SendWaiters);
            run.SendWait.Record(startedTimestamp);
            if (pong)
            {
                Interlocked.Decrement(ref run.PongWaiters);
                run.PongWait.Record(startedTimestamp);
            }
        }

        internal static void RecordReceiveBatch(long startedTimestamp, long bufferedBytes, long remainingBytes, int packets)
        {
            var run = Volatile.Read(ref _active);
            if (run == null) return;
            run.ReceiveBatch.Record(startedTimestamp);
            SetMaximum(ref run.BatchBytesPeak, bufferedBytes);
            SetMaximum(ref run.RemainingBytesPeak, remainingBytes);
            Interlocked.Add(ref run.Packets, packets);
            Interlocked.Add(ref run.ParsedBytes, Math.Max(0, bufferedBytes - remainingBytes));
        }

        internal static void RecordError(string category, Exception? error = null, int clientIndex = -1, string? detail = null)
        {
            var run = Volatile.Read(ref _active);
            if (run == null) return;
            var bucket = run.Errors.GetOrAdd(category, static _ => new ErrorCount());
            long count = Interlocked.Increment(ref bucket.Total);
            if (count <= bucket.Examples.Length)
            {
                string message = detail ?? error?.Message ?? "No detail";
                message = message.Replace('\r', ' ').Replace('\n', ' ');
                if (message.Length > 400) message = message[..400];
                string type = error?.GetType().Name ?? "event";
                Volatile.Write(ref bucket.Examples[(int)count - 1],
                    $"{DateTimeOffset.Now:O} client={clientIndex} {type}: {message}");
            }
        }

        private static void SetMaximum(ref long destination, long value)
        {
            long previous = Interlocked.Read(ref destination);
            while (value > previous)
            {
                long observed = Interlocked.CompareExchange(ref destination, value, previous);
                if (observed == previous) break;
                previous = observed;
            }
        }

        private sealed class Timing
        {
            private long _count, _ticks, _maxTicks, _over500, _over1000, _over5000;
            private long _previousCount, _previous500, _previous1000, _previous5000;

            internal void Record(long startedTimestamp)
            {
                long elapsed = Math.Max(0, Stopwatch.GetTimestamp() - startedTimestamp);
                Interlocked.Add(ref _ticks, elapsed);
                SetMaximum(ref _maxTicks, elapsed);
                if (elapsed > Stopwatch.Frequency / 2) Interlocked.Increment(ref _over500);
                if (elapsed > Stopwatch.Frequency) Interlocked.Increment(ref _over1000);
                if (elapsed > Stopwatch.Frequency * 5) Interlocked.Increment(ref _over5000);
                Interlocked.Increment(ref _count);
            }

            internal void Write(StreamWriter writer, string name)
            {
                long count = Interlocked.Read(ref _count);
                long ticks = Interlocked.Read(ref _ticks);
                long over500 = Interlocked.Read(ref _over500);
                long over1000 = Interlocked.Read(ref _over1000);
                long over5000 = Interlocked.Read(ref _over5000);
                double average = count == 0 ? 0 : ticks * 1000.0 / Stopwatch.Frequency / count;
                double maximum = Interlocked.Read(ref _maxTicks) * 1000.0 / Stopwatch.Frequency;
                writer.WriteLine(FormattableString.Invariant(
                    $"  {name}: count={count} (+{count - _previousCount}) avg_ms={average:F3} max_ms={maximum:F3} >500ms={over500} (+{over500 - _previous500}) >1s={over1000} (+{over1000 - _previous1000}) >5s={over5000} (+{over5000 - _previous5000})"));
                _previousCount = count;
                _previous500 = over500;
                _previous1000 = over1000;
                _previous5000 = over5000;
            }
        }

        private sealed class ErrorCount
        {
            internal long Total;
            internal long Reported;
            internal readonly string?[] Examples = new string?[2];
            internal readonly bool[] ExampleReported = new bool[2];
        }

        private sealed class Run : IDisposable
        {
            internal readonly string FilePath;
            internal string Phase = "starting";
            internal int Slots, Ready;
            internal long Pings, PongsCompleted, PongsFailed, SendWaiters, PongWaiters;
            internal long BatchBytesPeak, RemainingBytesPeak, Packets, ParsedBytes;
            internal readonly Timing PongLatency = new();
            internal readonly Timing SendWait = new();
            internal readonly Timing PongWait = new();
            internal readonly Timing ReceiveBatch = new();
            internal readonly ConcurrentDictionary<string, ErrorCount> Errors = new();

            private readonly StreamWriter _writer;
            private readonly Process _process;
            private readonly Thread _thread;
            private readonly ManualResetEvent _stop = new(false);
            private readonly object _lifecycleLock = new();
            private readonly ConsoleCancelEventHandler _cancelHandler;
            private readonly EventHandler _exitHandler;
            private long _previousTimestamp = Stopwatch.GetTimestamp();
            private TimeSpan _previousCpu;
            private long _previousAllocated = GC.GetTotalAllocatedBytes(false);
            private long _previousCompleted = ThreadPool.CompletedWorkItemCount;
            private long _previousPackets, _previousParsedBytes, _previousPings, _previousPongs, _previousPongFailures;
            private readonly int[] _previousGc = { GC.CollectionCount(0), GC.CollectionCount(1), GC.CollectionCount(2) };
            private int _disposed;

            internal Run(int requestedClients)
            {
                string directory = Path.Combine(AppContext.BaseDirectory, "Logs");
                Directory.CreateDirectory(directory);
                FilePath = Path.Combine(directory,
                    $"load-diagnostics-{DateTime.Now:yyyyMMdd-HHmmss-fff}-{Environment.ProcessId}.txt");
                _writer = new StreamWriter(new FileStream(FilePath, FileMode.CreateNew, FileAccess.Write, FileShare.Read), new UTF8Encoding(false));
                try
                {
                    _process = Process.GetCurrentProcess();
                    _previousCpu = _process.TotalProcessorTime;
                    _writer.WriteLine($"Dummy load diagnostics | started={DateTimeOffset.Now:O} pid={Environment.ProcessId} requested_clients={requestedClients}");
                    _writer.WriteLine($"CPU denominator: Environment.ProcessorCount={Environment.ProcessorCount}; cpu_percent=core_equivalents/denominator*100.");
                    _writer.WriteLine("Samples target 5 seconds on a dedicated background thread; actual interval is recorded (GC/OS pauses can delay it).");
                    _writer.WriteLine("Timing count/avg/max/threshold totals are cumulative; (+N) is the increase since the previous sample. Concurrent counters are approximate snapshots.");
                    _writer.WriteLine("PING latency: local PING frame dispatch start -> PONG socket SendAsync completion; excludes earlier TCP/Pipe/parsing backlog, network arrival, remote receipt, and network round-trip time.");
                    _writer.WriteLine("Buffer peaks: one socket ReadAsync batch and its unconsumed remainder; NOT the aggregate backlog of all pipes. Peaks are cumulative.");
                    _writer.WriteLine("slots/ready are simulator counters, NOT a count of live TCP connections. GC pause percentage is the runtime's cumulative GC statistic.");
                    _writer.WriteLine("Errors: fixed stage categories, cumulative counts and at most two examples per category for the whole run. One failure can appear at multiple stages; counts are NOT unique incidents. Memory units: MiB.");
                    _writer.Flush();
                    _thread = new Thread(ReportLoop) { IsBackground = true, Name = "Dummy diagnostics" };
                    _cancelHandler = (_, _) => Dispose();
                    _exitHandler = (_, _) => Dispose();
                }
                catch
                {
                    _writer.Dispose();
                    _process?.Dispose();
                    _stop.Dispose();
                    throw;
                }
            }

            internal void Start()
            {
                Console.CancelKeyPress += _cancelHandler;
                AppDomain.CurrentDomain.ProcessExit += _exitHandler;
                try { _thread.Start(); }
                catch
                {
                    Console.CancelKeyPress -= _cancelHandler;
                    AppDomain.CurrentDomain.ProcessExit -= _exitHandler;
                    _writer.Dispose();
                    _process.Dispose();
                    _stop.Dispose();
                    throw;
                }
            }

            public void Dispose()
            {
                lock (_lifecycleLock)
                {
                    if (_disposed != 0) return;
                    _disposed = 1;
                    Interlocked.CompareExchange(ref _active, null, this);
                    _stop.Set();
                }
                if (Thread.CurrentThread != _thread) _thread.Join();
            }

            private void ReportLoop()
            {
                try
                {
                    while (!_stop.WaitOne(TimeSpan.FromSeconds(5))) WriteSnapshot(false);
                    WriteSnapshot(true);
                }
                catch (Exception error)
                {
                    // Diagnostics must not take down the load generator when writing the report fails.
                    Console.Error.WriteLine($"Diagnostics disabled: {error.Message}");
                }
                finally
                {
                    lock (_lifecycleLock)
                    {
                        Interlocked.CompareExchange(ref _active, null, this);
                        _disposed = 1;
                        _stop.Dispose();
                    }
                    Console.CancelKeyPress -= _cancelHandler;
                    AppDomain.CurrentDomain.ProcessExit -= _exitHandler;
                    try { _writer.Dispose(); } catch { }
                    _process.Dispose();
                }
            }

            private void WriteSnapshot(bool final)
            {
                long now = Stopwatch.GetTimestamp();
                double seconds = Math.Max(0.000001, (now - _previousTimestamp) / (double)Stopwatch.Frequency);
                _process.Refresh();
                TimeSpan cpu = _process.TotalProcessorTime;
                double cores = (cpu - _previousCpu).TotalSeconds / seconds;
                string affinity = OperatingSystem.IsWindows() ? $"0x{(long)_process.ProcessorAffinity:X}" : "n/a";
                long allocated = GC.GetTotalAllocatedBytes(false);
                long completed = ThreadPool.CompletedWorkItemCount;
                GCMemoryInfo gc = GC.GetGCMemoryInfo();
                int gen0 = GC.CollectionCount(0), gen1 = GC.CollectionCount(1), gen2 = GC.CollectionCount(2);
                long pings = Interlocked.Read(ref Pings);
                long pongs = Interlocked.Read(ref PongsCompleted);
                long failed = Interlocked.Read(ref PongsFailed);
                long packets = Interlocked.Read(ref Packets);
                long parsedBytes = Interlocked.Read(ref ParsedBytes);
                const double mib = 1024.0 * 1024.0;

                _writer.WriteLine();
                _writer.WriteLine(FormattableString.Invariant($"[{DateTimeOffset.Now:O}] interval_s={seconds:F3} final={final} phase={Volatile.Read(ref Phase)} slots={Volatile.Read(ref Slots)} ready={Volatile.Read(ref Ready)}"));
                _writer.WriteLine(FormattableString.Invariant($"  process: cpu_percent={cores / Environment.ProcessorCount * 100:F2} core_equivalents={cores:F3} affinity_mask={affinity} working_MiB={_process.WorkingSet64 / mib:F1} private_MiB={_process.PrivateMemorySize64 / mib:F1}"));
                _writer.WriteLine(FormattableString.Invariant($"  GC: allocated_MiB_per_s={(allocated - _previousAllocated) / mib / seconds:F2} gen0=+{gen0 - _previousGc[0]} gen1=+{gen1 - _previousGc[1]} gen2=+{gen2 - _previousGc[2]} heap_MiB={gc.HeapSizeBytes / mib:F1} fragmented_MiB={gc.FragmentedBytes / mib:F1} pause_percent={gc.PauseTimePercentage:F2}"));
                _writer.WriteLine(FormattableString.Invariant($"  thread_pool: threads={ThreadPool.ThreadCount} pending={ThreadPool.PendingWorkItemCount} completed=+{completed - _previousCompleted}"));
                _writer.WriteLine($"  heartbeat: ping={pings} (+{pings - _previousPings}) pong_completed={pongs} (+{pongs - _previousPongs}) pong_failed={failed} (+{failed - _previousPongFailures}) unresolved={pings - pongs - failed}");
                _writer.WriteLine($"  send_lock_waiters: all={Interlocked.Read(ref SendWaiters)} pong={Interlocked.Read(ref PongWaiters)}");
                _writer.WriteLine(FormattableString.Invariant($"  receive: packets=+{packets - _previousPackets} parsed_MiB=+{(parsedBytes - _previousParsedBytes) / mib:F3} batch_bytes_peak={Interlocked.Read(ref BatchBytesPeak)} remainder_bytes_peak={Interlocked.Read(ref RemainingBytesPeak)}"));
                PongLatency.Write(_writer, "ping_to_pong");
                SendWait.Write(_writer, "send_lock_wait_all");
                PongWait.Write(_writer, "send_lock_wait_pong");
                ReceiveBatch.Write(_writer, "receive_batch");

                foreach (var pair in Errors.OrderBy(pair => pair.Key, StringComparer.Ordinal))
                {
                    var bucket = pair.Value;
                    long total = Interlocked.Read(ref bucket.Total);
                    _writer.WriteLine($"  error[{pair.Key}]: total={total} (+{total - bucket.Reported})");
                    bucket.Reported = total;
                    for (int i = 0; i < bucket.Examples.Length; ++i)
                    {
                        string? example = Volatile.Read(ref bucket.Examples[i]);
                        if (example == null || bucket.ExampleReported[i]) continue;
                        _writer.WriteLine($"    example: {example}");
                        bucket.ExampleReported[i] = true;
                    }
                }
                _writer.Flush();
                _previousTimestamp = now;
                _previousCpu = cpu;
                _previousAllocated = allocated;
                _previousCompleted = completed;
                _previousPings = pings;
                _previousPongs = pongs;
                _previousPongFailures = failed;
                _previousPackets = packets;
                _previousParsedBytes = parsedBytes;
                _previousGc[0] = gen0;
                _previousGc[1] = gen1;
                _previousGc[2] = gen2;
            }
        }

        private sealed class EmptyLease : IDisposable
        {
            internal static readonly EmptyLease Instance = new();
            public void Dispose() { }
        }
    }
}

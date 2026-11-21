using System.Collections.Concurrent;
using System.Diagnostics;
using System.Net;
using System.Net.Http.Json;
using System.Net.Sockets;
using System.Text.Json;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Hosting.Server;
using Microsoft.AspNetCore.Hosting.Server.Features;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace ServerDependencyAnalyzer;

internal record LocalReportService(string Endpoint, string Id);
internal record ReanalysisRequest(AnalysisOptions Options, string Output, string? LatestAlias = null);

internal static class ReanalysisService
{
    private record ServiceState(string Endpoint, string Key);
    private record Job(string State, string Progress, string? Error = null, string? Report = null);
    private record SourceRequest(string File, int Line);
    private sealed class ProgressCallback(Action<string> update) : IProgress<string>
    {
        public void Report(string value) => update(value);
    }

    private static string RequestsDirectory => Path.Combine(AppContext.BaseDirectory, "Reanalysis");
    private static string StateFile => Path.Combine(AppContext.BaseDirectory, "LocalService.json");
    private static readonly HttpClient Client = new() { Timeout = TimeSpan.FromSeconds(1) };

    public static LocalReportService Register(AnalysisOptions options, string output, string? latestAlias)
    {
        var endpoint = EnsureRunning();
        var id = Guid.NewGuid().ToString("N");
        Directory.CreateDirectory(RequestsDirectory);
        File.WriteAllText(Path.Combine(RequestsDirectory, id + ".json"),
            JsonSerializer.Serialize(new ReanalysisRequest(options, Path.GetFullPath(output), latestAlias)));
        return new(endpoint, id);
    }

    public static string EnsureRunning()
    {
        var state = ReadState();
        if (Alive(state)) return state!.Endpoint;
        var executable = Path.Combine(AppContext.BaseDirectory, "ServerDependencyAnalyzer.exe");
        var start = new ProcessStartInfo(executable) { UseShellExecute = false, CreateNoWindow = true };
        start.ArgumentList.Add("--serve");
        using var process = Process.Start(start) ?? throw new InvalidOperationException("로컬 분석 서버를 시작하지 못했습니다.");
        var until = Environment.TickCount64 + 8000;
        do
        {
            Thread.Sleep(100);
            state = ReadState();
            if (Alive(state)) return state!.Endpoint;
            if (process.HasExited && process.ExitCode != 0) break;
        } while (Environment.TickCount64 < until);
        throw new InvalidOperationException("로컬 분석 서버에 연결하지 못했습니다. --start-service로 다시 시작하세요.");
    }

    public static void Stop()
    {
        var state = ReadState();
        if (!Alive(state)) return;
        using var request = new HttpRequestMessage(HttpMethod.Post, state!.Endpoint + "/shutdown");
        request.Headers.Add("X-Service-Key", state.Key);
        using var response = Client.Send(request);
        response.EnsureSuccessStatusCode();
        var until = Environment.TickCount64 + 10000;
        while (Alive(state) && Environment.TickCount64 < until) Thread.Sleep(100);
        if (Alive(state)) throw new InvalidOperationException("현재 분석을 취소하고 서버가 종료되기를 기다리는 중입니다.");
    }

    private static ServiceState? ReadState()
    {
        try
        {
            var state = JsonSerializer.Deserialize<ServiceState>(File.ReadAllText(StateFile));
            return state is not null && Uri.TryCreate(state.Endpoint, UriKind.Absolute, out var uri)
                && uri.Scheme == "http" && uri.Host == "127.0.0.1" && uri.AbsolutePath == "/"
                && uri.Query.Length == 0 && uri.Fragment.Length == 0 && uri.UserInfo.Length == 0 ? state : null;
        }
        catch (Exception exception) when (exception is IOException or JsonException or UnauthorizedAccessException) { return null; }
    }

    private static bool Alive(ServiceState? state)
    {
        if (state is null) return false;
        try { return Client.GetStringAsync(state.Endpoint + "/health").GetAwaiter().GetResult() == state.Key; }
        catch (Exception exception) when (exception is HttpRequestException or TaskCanceledException) { return false; }
    }

    private static ReanalysisRequest Resolve(string id)
    {
        if (!Guid.TryParseExact(id, "N", out var parsed)) throw new ArgumentException("잘못된 보고서 ID입니다.");
        var path = Path.Combine(RequestsDirectory, parsed.ToString("N") + ".json");
        if (!File.Exists(path)) throw new ArgumentException("이 분석기에 저장된 보고서 설정이 없습니다. 새로 분석하세요.");
        return JsonSerializer.Deserialize<ReanalysisRequest>(File.ReadAllText(path))
            ?? throw new ArgumentException("저장된 분석 설정을 읽지 못했습니다.");
    }

    public static async Task Run()
    {
        Directory.CreateDirectory(RequestsDirectory);
        FileStream ownership;
        try { ownership = File.Open(Path.Combine(RequestsDirectory, "service.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None); }
        catch (IOException) { return; }
        using (ownership)
        {
            var previous = ReadState();
            var port = previous is null ? 0 : new Uri(previous.Endpoint).Port;
            if (port > 0)
            {
                var probe = new TcpListener(IPAddress.Loopback, port);
                try { probe.Start(); }
                catch (SocketException) { port = 0; }
                finally { probe.Stop(); }
            }
            var builder = WebApplication.CreateSlimBuilder(new WebApplicationOptions { ContentRootPath = AppContext.BaseDirectory });
            builder.Logging.ClearProviders();
            builder.WebHost.ConfigureKestrel(server =>
            {
                server.Listen(IPAddress.Loopback, port);
                server.Limits.MaxRequestBodySize = 4096;
            });
            await using var app = builder.Build();
            var key = Guid.NewGuid().ToString("N");
            var endpoint = "";
            var jobs = new ConcurrentDictionary<string, Job>();
            using var serial = new SemaphoreSlim(1);
            Task? running = null;
            app.Use(async (context, next) =>
            {
                context.Response.Headers.CacheControl = "no-store";
                if (context.Request.Path == "/health" || context.Request.Path == "/shutdown") { await next(context); return; }
                // Browser actions run in the daemon's report page, on the same loopback origin.
                var origin = context.Request.Headers.Origin.ToString();
                if ((origin.Length > 0 && origin != endpoint)
                    || (HttpMethods.IsPost(context.Request.Method) && origin != endpoint))
                {
                    context.Response.StatusCode = 403;
                    return;
                }
                try { await next(context); }
                catch (Exception exception) when (exception is ArgumentException or IOException or JsonException or UnauthorizedAccessException)
                {
                    context.Response.StatusCode = 400;
                    await context.Response.WriteAsJsonAsync(new { error = exception.Message });
                }
            });
            app.MapGet("/health", () => key);
            app.MapGet("/reports/{id}", (string id) =>
            {
                var request = Resolve(id);
                return Results.File(request.Output, "text/html; charset=utf-8");
            });
            app.MapPost("/shutdown", (HttpContext context) =>
            {
                if (context.Request.Headers["X-Service-Key"] != key) return Results.StatusCode(403);
                context.Response.OnCompleted(() => { app.Lifetime.StopApplication(); return Task.CompletedTask; });
                return Results.Ok();
            });
            app.MapGet("/api/jobs/{id}", (string id) =>
            {
                _ = Resolve(id);
                return jobs.GetValueOrDefault(id) ?? new Job("idle", "재분석 대기");
            });
            app.MapPost("/api/reanalyze/{id}", (string id) =>
            {
                var request = Resolve(id);
                if (!serial.Wait(0)) return Results.Conflict(new { error = "다른 재분석이 진행 중입니다. 완료 후 다시 요청하세요." });
                jobs[id] = new("running", "분석 시작…");
                running = Task.Run(() =>
                {
                    try
                    {
                        var progress = new ProgressCallback(message => jobs.AddOrUpdate(id,
                            new Job("running", message), (_, job) => job with { Progress = message }));
                        var result = Analyzer.Run(request.Options, progress, app.Lifetime.ApplicationStopping);
                        app.Lifetime.ApplicationStopping.ThrowIfCancellationRequested();
                        var alias = HtmlReport.IsCurrentReport(request.LatestAlias, id) ? request.LatestAlias : null;
                        var report = HtmlReport.Write(result, request.Output, alias);
                        jobs[id] = new("completed", $"분석 완료 · 성공 {result.Parsed}/{result.Sources} · 진단 {result.Failures.Count}",
                            Report: new Uri(report).AbsoluteUri);
                    }
                    catch (Exception exception) { jobs[id] = new("failed", "재분석 실패", exception.Message); }
                    finally { serial.Release(); }
                });
                return Results.Accepted();
            });
            app.MapPost("/api/source/{id}", async (string id, HttpContext context) =>
            {
                var request = Resolve(id);
                var source = await context.Request.ReadFromJsonAsync<SourceRequest>();
                if (source is null || source.Line < 1) return Results.BadRequest(new { error = "소스 위치를 확인하세요." });
                var link = "cppdeps://open/?root=" + Uri.EscapeDataString(request.Options.Root)
                    + "&file=" + Uri.EscapeDataString(source.File) + "&line=" + source.Line;
                // Visual Studio COM automation needs its own STA thread in the HTTP server.
                var opened = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
                var thread = new Thread(() =>
                {
                    try { opened.SetResult(VisualStudioLink.Open(link)); }
                    catch (Exception exception) { opened.SetException(exception); }
                }) { IsBackground = true };
                thread.SetApartmentState(ApartmentState.STA);
                thread.Start();
                try { return Results.Ok(new { opened = await opened.Task }); }
                catch (Exception exception) { return Results.BadRequest(new { error = exception.Message }); }
            });
            await app.StartAsync();
            endpoint = app.Services.GetRequiredService<IServer>().Features.Get<IServerAddressesFeature>()!.Addresses.Single();
            File.WriteAllText(StateFile, JsonSerializer.Serialize(new ServiceState(endpoint, key)));
            await app.WaitForShutdownAsync();
            if (running is not null) await running;
        }
    }
}

using System;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Newtonsoft.Json;
using static ClientCore.Config;

namespace ClientCore.Services
{
    public class LoginResponse
    {
        public string AccessToken { get; set; }
        public ulong UserID { get; set; }
    }
    public class GameServerInfo
    {
        public string IP { get; set; }
        public int Port { get; set; }
    }
    public class SessionResponse
    {
        public string SessionToken { get; set; }
        public GameServerInfo GameServer { get; set; }
    }
    internal class AuthService
    {
        public static AuthService Instance { get; } = new AuthService();
        internal async Task<LoginResponse> LoginAsync(string id, string pwd, CancellationToken cancellationToken)
        {
            using (var client = new HttpClient())
            using (var content = new StringContent(JsonConvert.SerializeObject(new { login_id = id, password = pwd }), Encoding.UTF8, "application/json"))
            using (var response = await client.PostAsync(LOGIN_SERVER_ADDR, content, cancellationToken).ConfigureAwait(false))
            {
                if (!response.IsSuccessStatusCode)
                    throw new InvalidOperationException($"Login failed (HTTP {(int)response.StatusCode}).");
                LoginResponse parsed;
                try { parsed = JsonConvert.DeserializeObject<LoginResponse>(await response.Content.ReadAsStringAsync().ConfigureAwait(false)); }
                catch (JsonException) { throw new InvalidOperationException("Invalid login response."); }
                if (parsed == null || string.IsNullOrWhiteSpace(parsed.AccessToken) || parsed.UserID == 0)
                    throw new InvalidOperationException("Invalid login response.");
                cancellationToken.ThrowIfCancellationRequested();
                return parsed;
            }
        }
        internal async Task<SessionResponse> GetSessionAsync(string accessToken, CancellationToken cancellationToken)
        {
            using (var client = new HttpClient())
            {
                client.DefaultRequestHeaders.Authorization = new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", accessToken);
                using (var response = await client.PostAsync("http://127.0.0.1:3000/auth/getSession", null, cancellationToken).ConfigureAwait(false))
                {
                    if (!response.IsSuccessStatusCode)
                        throw new InvalidOperationException($"Session request failed (HTTP {(int)response.StatusCode}).");
                    SessionResponse parsed;
                    try { parsed = JsonConvert.DeserializeObject<SessionResponse>(await response.Content.ReadAsStringAsync().ConfigureAwait(false)); }
                    catch (JsonException) { throw new InvalidOperationException("Invalid session response."); }
                    if (parsed == null || string.IsNullOrWhiteSpace(parsed.SessionToken) || Encoding.UTF8.GetByteCount(parsed.SessionToken) > 36 ||
                        parsed.GameServer == null || !IPAddress.TryParse(parsed.GameServer.IP, out _) || parsed.GameServer.Port < 1 || parsed.GameServer.Port > 65535)
                        throw new InvalidOperationException("Invalid session response.");
                    cancellationToken.ThrowIfCancellationRequested();
                    return parsed;
                }
            }
        }
    }
}

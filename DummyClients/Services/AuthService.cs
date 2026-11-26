using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using static ClientCore.Config;
using static System.Collections.Specialized.BitVector32;

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
        private static readonly HttpClient Client = new(new SocketsHttpHandler
        {
            MaxConnectionsPerServer = 100
        }) { Timeout = TimeSpan.FromSeconds(30) };

        public static AuthService Instance { get; } = new AuthService();
        internal async Task LoginAsync(ClientSession session, string id, string pwd)
        {

            var requestData = new
            {
                login_id = id,
                password = pwd
            };

            string json = JsonConvert.SerializeObject(requestData);

            using var content = new StringContent(json, Encoding.UTF8, "application/json");

            using HttpResponseMessage response = await Client.PostAsync(LOGIN_SERVER_ADDR, content);

            if (response.IsSuccessStatusCode)
            {
                string responseBody = await response.Content.ReadAsStringAsync();

                var parsed = JsonConvert.DeserializeObject<LoginResponse>(responseBody);
                string accessToken = parsed?.AccessToken;
                ulong userID = parsed?.UserID ?? 0;
                if (!string.IsNullOrEmpty(accessToken))
                {
                    session.AccessToken = accessToken;
                    session.UserID = userID;
                }
                else
                {
                    throw new Exception("로그인 실패: 토큰이 없음.");
                }
            }
            else
            {
                throw new Exception($"로그인 실패: {response.StatusCode}");
            }
        }
        public async Task<(string serverAddress, int serverPort)> GetSessionAsync(ClientSession session)
        {

            using var request = new HttpRequestMessage(HttpMethod.Post, "http://127.0.0.1:3000/auth/getSession");
            request.Headers.Authorization = new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", session.AccessToken);

            using HttpResponseMessage response = await Client.SendAsync(request);
            GameServerInfo gameServer;
            if (response.IsSuccessStatusCode)
            {
                string responseBody = await response.Content.ReadAsStringAsync();

                var parsed = JsonConvert.DeserializeObject<SessionResponse>(responseBody);
                string sessionToken = parsed?.SessionToken;
                gameServer = parsed?.GameServer;
                if (!string.IsNullOrEmpty(sessionToken) && gameServer != null && !string.IsNullOrEmpty(gameServer.IP) && gameServer.Port > 0 && gameServer.Port <= 65535)
                {
                    session.SessionToken = sessionToken;
                }
                else
                {
                    throw new Exception("세션토큰이 없음");
                }
            }
            else
            {
                throw new Exception($"세션 토큰 요청 실패: {response.StatusCode}");
            }
            return (gameServer.IP, gameServer.Port);
        }
    }
}

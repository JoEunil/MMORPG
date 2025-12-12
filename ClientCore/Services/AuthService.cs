using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net.Http;

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
        internal async Task LoginAsync(string id, string pwd)
        {

            using (HttpClient client = new HttpClient())
            {
                var requestData = new
                {
                    username = id,
                    password = pwd
                };

                string json = JsonConvert.SerializeObject(requestData);

                var content = new StringContent(json, Encoding.UTF8, "application/json");

                HttpResponseMessage response = await client.PostAsync(LOGIN_SERVER_ADDR, content);
                 
                if (response.IsSuccessStatusCode)
                {
                    string responseBody = await response.Content.ReadAsStringAsync();

                    var parsed = JsonConvert.DeserializeObject<LoginResponse>(responseBody);
                    string accessToken = parsed?.AccessToken;
                    ulong userID = parsed.UserID;
                    if (!string.IsNullOrEmpty(accessToken))
                    {
                        ClientSession.AccessToken = accessToken;
                        ClientSession.UserID = userID;
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
        }
        public async Task<(string serverAddress, int serverPort)> GetSessionAsync()
        {

            using (HttpClient client = new HttpClient())
            {
                client.DefaultRequestHeaders.Authorization = new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", ClientSession.AccessToken);

                HttpResponseMessage response = await client.PostAsync("http://127.0.0.1:3000/auth/getSession", null);
                GameServerInfo gameServer;
                if (response.IsSuccessStatusCode)
                {
                    string responseBody = await response.Content.ReadAsStringAsync();

                    var parsed = JsonConvert.DeserializeObject<SessionResponse>(responseBody);
                    string sessionToken = parsed?.SessionToken;
                    gameServer = parsed?.GameServer;
                    if (!string.IsNullOrEmpty(sessionToken))
                    {
                        ClientSession.SessionToken = sessionToken;
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
}
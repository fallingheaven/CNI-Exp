using System.Net;
using System.Net.Sockets;
using System.Text;

namespace E5_3790;

class LicenseServer
{
    private const int TcpPort = 12345;
    private const int HttpPort = 4430;
    private Dictionary<string, LicenseData> _licenseUsage = new ();

    private HttpClient _client;
    
    // 启动服务器
    public void Start()
    {
        _client = new HttpClient();
        
        var listener = new TcpListener(IPAddress.Any, TcpPort);
        listener.Start();
        Console.WriteLine("License Server started...");

        while (true)
        {
            // 接受客户端连接
            var client = listener.AcceptTcpClient();
            ThreadPool.QueueUserWorkItem(HandleClient, client);
        }
    }

    // 对tcp报文进行处理
    private void HandleClient(object? obj)
    {
        if (obj is not TcpClient tcpClient)
        {
            Console.WriteLine("未知消息");
            return;
        }

        var stream = tcpClient.GetStream();
        var buffer = new byte[256];

        // 超时任务
        var cts = new CancellationTokenSource();
        var timeoutTask = WaitForTimeout(cts.Token, tcpClient);

        try
        {
            // 读取客户端请求
            while (true)
            {
                if (!tcpClient.Connected) break;

                int bytesRead;
                while (stream.CanRead && (bytesRead = stream.Read(buffer, 0, buffer.Length)) != 0)
                {
                    var request = Encoding.UTF8.GetString(buffer, 0, bytesRead).Split(' ');
                    var response = "";

                    cts.Cancel();
                    cts.Dispose();

                    Console.WriteLine($"收到请求: {request[0]}");
                    switch (request[0])
                    {
                        case "heartbeat":
                        {
                            break;
                        }
                        case "register":
                        {
                            response = HandleRegisterRequest(request[1]).Result;
                            break;
                        }
                        case "seek":
                        {
                            response = HandleSeekRequest().Result;
                            break;
                        }
                        case "verify":
                        {
                            response = HandleVerifyRequest(request[1]).Result;
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }

                    Console.WriteLine(response);
                    var responseBytes = Encoding.UTF8.GetBytes(response);
                    stream.Write(responseBytes, 0, responseBytes.Length);

                    // 重新计时
                    cts = new CancellationTokenSource();
                    timeoutTask = WaitForTimeout(cts.Token, tcpClient);
                }
            }
        }
        catch (IOException ex)
        {
            Console.WriteLine(ex.Message);
        }
        catch (SocketException ex)
        {
            Console.WriteLine(ex.Message);
        }
        catch (Exception ex)
        {
            Console.WriteLine(ex.Message);
        }
        finally
        {
            tcpClient.Close();
        }
    }
    
    // 等5秒，如果超时就认定程序A崩溃
    private async Task WaitForTimeout(CancellationToken token, TcpClient tcpClient)
    {
        try
        {
            await Task.Delay(5000, token);
            Console.WriteLine("5秒内未收到心跳，断开连接");
           
            tcpClient.Close();
        }
        catch (TaskCanceledException)
        {
            
        }
    }

    private async Task<string> HandleRegisterRequest(string licenseString)
    {
        var url = "https://visionary.net.cn:" + HttpPort + "/network_exp_5/api/register.php";
        var postData = new FormUrlEncodedContent(new[]
        {
            new KeyValuePair<string, string>("license", licenseString)
        });
        
        var response = await _client.PostAsync(url, postData);
        var responseString = await response.Content.ReadAsStringAsync();
        
        return responseString;
    }
    
    private async Task<string> HandleSeekRequest()
    {
        var url = "https://visionary.net.cn:" + HttpPort + "/network_exp_5/api/seek.php";

        var response = await _client.PostAsync(url, null);
        var responseString = await response.Content.ReadAsStringAsync();
        
        return responseString;
    }

    private async Task<string> HandleVerifyRequest(string serialNumber)
    {
        var url = "https://visionary.net.cn:" + HttpPort + "/network_exp_5/api/verify.php";
        // var postData = new StringContent(serialNumber, Encoding.UTF8, "application/json");
        var postData = new FormUrlEncodedContent(new[]
        {
            new KeyValuePair<string, string>("serial_num", serialNumber)
        });
        
        var response = await _client.PostAsync(url, postData);
        var responseString = await response.Content.ReadAsStringAsync();
        
        return responseString;
    }
}
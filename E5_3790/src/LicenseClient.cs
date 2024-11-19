using System.Net.Sockets;
using System.Text;

namespace E5_3790;

class LicenseClient
{
    private const string ServerAddress = "127.0.0.1";  // 服务器地址
    private const int ServerPort = 12345;              // 服务器端口
    private TcpClient? _client;
    private Timer? _heartbeatTimer;

    // 启动程序A
    public void Start()
    {
        try
        {
            ConnectToServer();
            var stream = _client.GetStream();

            var (seekRes, seekInfo) = Communicate(stream, "seek");
            Console.WriteLine(seekInfo);

            // 没有注册过序列号
            if (seekRes == 0)
            {
                Console.WriteLine("请输入序列号进行激活：");
                // 
                var serialNum = Console.ReadLine();

                var (verifyRes, verifyInfo) = Communicate(stream, $"verify {serialNum}");
                Console.WriteLine(verifyInfo);

                if (verifyRes != 1)
                {
                    Close(_client, stream);
                    return;
                }
                else
                {
                    Run();
                }
            }
            else
            {
                Console.WriteLine("登录成功");
                Run();
            }

            
        }
        catch (SocketException ex)
        {
            Console.WriteLine($"连接服务器失败: {ex.Message}");
            Console.WriteLine("重连中...");
        }
        catch (IOException ex)
        {
            Console.WriteLine($"服务器崩溃: {ex.Message}");
            Console.WriteLine("重连中...");
        }
    }

    // 程序A运行
    private void Run()
    {
        var threadStop = false;
        // 时刻检查是否掉线
        var thread = new Thread(() =>
        {
            while (true)
            {
                if (threadStop) return;

                if (_client == null || !_client.Connected)
                {
                    ConnectToServer();
                    Console.WriteLine("重连成功");
                }
            }
        });
        thread.Start();
        
        Console.WriteLine("应用运行中，q键退出");
        
        while (true)
        {
            // 其他工作
            
            var key = Console.ReadKey(intercept: true);

            if (key.Key == ConsoleKey.Q)
            {
                Console.WriteLine("退出应用");
                threadStop = true;
                
                if (_client is { Connected: true })
                {
                    Close(_client, _client.GetStream());
                }
                break;
            }
        }
    }

    // 连接到服务器
    private void ConnectToServer()
    {
        while (_client == null || !_client.Connected)
        {
            try
            {
                _client = new TcpClient(ServerAddress, ServerPort);
                _heartbeatTimer = new Timer(SendHeartbeat, _client, 1000, 4000);
            }
            catch (SocketException e)
            {
                Console.WriteLine($"服务器连接失败:{e.Message}");
            }
        }
    }

    // 发送心跳信息，确认程序存活
    private void SendHeartbeat(object? obj)
    {
        try
        {
            if (obj is not TcpClient tcpClient) return;
            if (!tcpClient.Connected) return;
            
            var stream = tcpClient.GetStream();
            
            var heartbeatMessage = "heartbeat";
            var buffer = Encoding.UTF8.GetBytes(heartbeatMessage);
            
            stream.Write(buffer, 0, buffer.Length);
            stream.Flush();
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
        }
    }

    // 封装对发送tcp报文以及接收对应的服务器的信息
    private KeyValuePair<int, string> Communicate(NetworkStream stream, string content)
    {
        try
        {
            var buffer = Encoding.UTF8.GetBytes(content);
            
            stream.Write(buffer, 0, buffer.Length);
            stream.Flush();

            // 接收授权结果
            buffer = new byte[256];
            int bytesRead = 0;
            while ((bytesRead = stream.Read(buffer, 0, buffer.Length)) <= 0) {}
            var response = Encoding.UTF8.GetString(buffer, 0, bytesRead);
                
            var segments = response.Split(' ');
            if (!int.TryParse(segments[0], out var result)) result = 0;
            var info = segments[1];
            
            return new KeyValuePair<int, string>(result, info);
        }
        catch (Exception e)
        {
            Console.WriteLine(e.Message);
            return new KeyValuePair<int, string>(0, e.Message);
        }
    }

    // 关闭和服务器的tcp连接
    private void Close(TcpClient client, NetworkStream stream)
    {
        Console.WriteLine("程序将在2秒后关闭");
        Thread.Sleep(2000);
        stream.Close();
        client.Close();
    }
}
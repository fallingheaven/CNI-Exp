using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace E5_3790;

[Serializable]
public class LicenseData
{
    public string SerialNumber { get; set; }
    public string UserName { get; set; }
    public string Password { get; set; }
    public string Type { get; set; }
    public int MaxUsers { get; set; }
}

public class LicenseManager
{
    private const string ServerAddress = "127.0.0.1";  // 服务器地址
    private const int ServerPort = 12345;              // 服务器端口
    private TcpClient? _client;
    private Timer? _heartbeatTimer;
    // 用于生成许可证的密钥
    private const string SecretKey = "WQC200437";
    private readonly Dictionary<string, int> _typeToMaxUsers = new()
    {
        {"basic", 10},
        {"pro", 50}
    };

    // 启动许可证程序
    public void Start()
    {
        ConnectToServer();
        
        var stream = _client.GetStream();
        
        while (true)
        {
            Console.WriteLine("输入用户名、口令、许可证类型获取序列号");
            var input = Console.ReadLine()?.Split(" ");
            if (input != null)
            {
                var licenseData = GenerateLicense(input);
                if (licenseData == null) continue;
                
                var jsonString = JsonSerializer.Serialize(licenseData);
                
                var (registerRes, registerInfo) = Communicate(stream, $"register {jsonString}");
                Console.WriteLine(registerInfo);
                
                if (registerRes == 1)
                {
                    Console.WriteLine($"您的序列号为{licenseData.SerialNumber}，可激活主机数上限为{licenseData.MaxUsers}人");
                }
            }
            
            Console.WriteLine("输入空格后继续，输入q退出");
            var quit = false;
            while (true)
            {
                var key = Console.ReadKey(intercept: true);

                if (key.Key == ConsoleKey.Q)
                {
                    quit = true;
                    Close(_client, stream);
                    break;
                }

                if (key.Key == ConsoleKey.Spacebar)
                {
                    Console.WriteLine("\n");
                    break;
                }
            }
            
            if (quit) break;
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
            var stream = tcpClient.GetStream();
            if (!tcpClient.Connected) return;
            
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
    
    // 生成许可证序列号
    private LicenseData GenerateLicense(string[] inputString)
    {
        // 检查输入是否为空
        if (inputString.Length < 3 || string.IsNullOrEmpty(inputString[0]) || string.IsNullOrEmpty(inputString[1]) || string.IsNullOrEmpty(inputString[2]))
        {
            Console.WriteLine("用户名、口令和许可证类型不能为空！");
            return null;
        }
        var username = inputString[0];
        var password = inputString[1];
        var licenseType = inputString[2];

        // 拼接输入信息
        var input = $"{username} {password} {licenseType} {DateTime.UtcNow.Ticks}";

        // 生成哈希值
        var hmac = new HMACSHA256(Encoding.UTF8.GetBytes(SecretKey));
        var hashBytes = hmac.ComputeHash(Encoding.UTF8.GetBytes(input));
        var serialNumber = BitConverter.ToString(hashBytes).Replace("-", "").Substring(0, 20);

        var data = new LicenseData()
        {
            SerialNumber = serialNumber,
            UserName = username,
            Password = password,
            Type = licenseType,
            MaxUsers = _typeToMaxUsers[licenseType],
        };

        return data;
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

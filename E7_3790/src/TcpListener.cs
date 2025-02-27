using System;
using System.IO;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading;
using System.Text;

namespace E7_3790;

class ProxyServer
{
    private readonly string _host = "0.0.0.0"; // 监听所有接口
    private int _port = 1080; // SOCKS5代理的端口
    // private readonly string _host = "192.168.31.26"; // 监听所有接口
    // private readonly int _port = 1080; // SOCKS5代理的端口

    public void Start(int port)
    {
        _port = port;
        var listener = new TcpListener(IPAddress.Parse(_host), _port);
        listener.Start();
        Console.WriteLine($"SOCKS5 Proxy Server is listening on {_host}:{_port}");

        while (true)
        {
            try
            {
                // 等待客户端连接
                var client = listener.AcceptTcpClient();
                // Console.WriteLine("Client connected.");

                // 处理每个连接
                ThreadPool.QueueUserWorkItem(HandleClient, client);
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error accepting client connection: " + ex.Message);
            }
        }
    }
    
    private void HandleClient(object obj)
    {
        var client = (TcpClient)obj;
        var networkStream = client.GetStream();
        var reader = new BinaryReader(networkStream);
        var writer = new BinaryWriter(networkStream);
        
        try
        {
            var version = reader.ReadByte();
            if (version == 0x05) // SOCKS5
            {
                HandleSocks5(reader, writer, networkStream, client);
            }
            else if (version == 0x04) // SOCKS4
            {
                // HandleSocks4(reader, writer, networkStream, client);
            }
            else
            {
                throw new Exception($"Unsupported SOCKS version: {version}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine("Error handling client: " + ex.Message);
        }
        finally
        {
            // Console.WriteLine("Client disconnected.");
            // client.Close();
        }
    }
    

    private void HandleSocks5(BinaryReader reader, BinaryWriter writer, NetworkStream networkStream, TcpClient client)
    {
        // Console.WriteLine("Socks5 received.");
        try
        {
            // 处理SOCKS5握手协议
            // var version = reader.ReadByte();
            var nMethods = reader.ReadByte();
            reader.ReadBytes(nMethods); // 忽略支持的认证方法

            // 发送SOCKS5响应
            writer.Write((byte)5); // SOCKS5版本
            writer.Write((byte)0); // 无需认证
            writer.Flush();

            // 读取客户端请求（连接目标）
            byte version = reader.ReadByte(); // 版本，为0x05
            byte cmd = reader.ReadByte(); // 命令，1表示连接
            byte rev = reader.ReadByte(); // 保留字段
            byte addrType = reader.ReadByte();
            
            string host;
            switch (addrType)
            {
                // IPv4
                case 1:
                {
                    var ip = reader.ReadBytes(4);
                    host = new IPAddress(ip).ToString();
                    break;
                }
                // 域名
                case 3:
                {
                    var dnLen = reader.ReadByte();
                    host = new string(reader.ReadChars(dnLen));
                    break;
                }
                // IPv6
                case 4:
                {
                    var ip = reader.ReadBytes(16);
                    host = new IPAddress(ip).ToString();
                    break;
                }
                default:
                    // Console.WriteLine("Unknown address type: " + addrType);
                    return;
                    throw new Exception("Unsupported address type.");
            }

            int port = reader.ReadByte() << 8 | reader.ReadByte();
            
            // Console.WriteLine($"报文信息 {host}:{port} {version} {nMethods} {addrType}");

            // Console.WriteLine($"目标服务器连通性：{host}:{port} 结果：{IsHostReachable(host)}");
            if (!IsHostReachable(host)) return;
            
            // 建立与目标服务器的连接
            var targetClient = new TcpClient(host, port);
            var targetStream = targetClient.GetStream();
            // Console.WriteLine($"建立了一个和网站的tcp连接 {host}:{port} {targetStream.Socket.RemoteEndPoint}");

            // 发送成功响应
            byte[] successResponse = new byte[] { 0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
            networkStream.Write(successResponse, 0, successResponse.Length);
            
            ForwardData(client, targetClient);
            
        }
        catch (Exception ex)
        {
            Console.WriteLine("Error handling client: " + ex.Message);
        }
    }
    

    private void ForwardData(TcpClient srcClient, TcpClient destClient)
    {
        var sourceStream = srcClient.GetStream();
        var destinationStream = destClient.GetStream();
        
        try
        {
            byte[] buffer = new byte[8192]; // 8KB 缓冲区
    
            while (true)
            {
                if (sourceStream.DataAvailable)
                {
                    int bytesRead = sourceStream.Read(buffer, 0, buffer.Length);
                    if (bytesRead > 0) // 没有数据可读，可能连接已关闭
                    {
                        destinationStream.Write(buffer, 0, bytesRead); // 转发数据
                        destinationStream.Flush(); // 确保数据立即发送
                    }
                }
                
                if (destinationStream.DataAvailable)
                {
                    int bytesRead = destinationStream.Read(buffer, 0, buffer.Length);
                    if (bytesRead > 0) // 没有数据可读，可能连接已关闭
                    {
                        sourceStream.Write(buffer, 0, bytesRead); // 转发数据
                        sourceStream.Flush(); // 确保数据立即发送
                    }
                }
            }
        }
        catch (ObjectDisposedException ex)
        {
            Console.WriteLine($"Stream has been closed: " + ex.Message);
        }
        catch (IOException ex)
        {
            // 处理连接问题，目标主机强制关闭连接
            Console.WriteLine($"IO Error: " + ex.Message);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error forwarding data: " + ex.Message);
        }
        finally
        {
            srcClient.Close();
            destClient.Close();
        }
    }

    
    public static bool IsHostReachable(string host)
    {
        try
        {
            Ping ping = new Ping();
            PingReply reply = ping.Send(host);
            return reply.Status == IPStatus.Success;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Ping failed: {ex.Message}");
            return false;
        }
    }
}
using E7_3790;

string port = args[2];
if (int.TryParse(port, out int portInt))
{
    var proxyServer = new ProxyServer();
    proxyServer.Start(portInt);
}
else
{
    Console.WriteLine("请输入正确的参数");
}

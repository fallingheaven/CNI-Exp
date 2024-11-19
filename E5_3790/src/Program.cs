using E5_3790;

if (args[0] == "server")
{
    var server = new LicenseServer();
    server.Start();
}

if (args[0] == "client")
{
    var client = new LicenseClient();
    client.Start();
}

if (args[0] == "license")
{
    var manager = new LicenseManager();
    manager.Start();
}
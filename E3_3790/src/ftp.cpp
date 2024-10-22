#include <pcap.h>
#include <cstdio>
#include <ctime>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <map>
#include <array>
#include <iostream>
#include <ostream>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

// 以太网帧结构
struct ethernet_header
{
    std::array<u_char, 6> dstMac;
    std::array<u_char, 6> srcMac;
    u_short type;
};

// IP 头结构
struct ip_header
{
    u_char ver_hl;              // 版本 (4 bits) + 头长 (4 bits)
    u_char service;             // 服务类型
    u_short totalLen;           // 总长
    u_short identification;     // 标识
    u_short flags_fragOffset;   // 标志 (3 bits) + 片偏移 (13 bits)
    u_char ttl;                 // 生存时间
    u_char proto;               // 协议
    u_short checksum;           // 头校验和
    in_addr srcAddr;            // 源地址
    in_addr dstAddr;            // 目标地址
};

struct InAddrCompare
{
    bool operator()(const in_addr &a, const in_addr &b) const
    {
        return memcmp(&a, &b, sizeof(in_addr)) < 0;
    }
};

FILE *logfile;
std::map<in_addr, u_int, InAddrCompare> data_from_ip, data_to_ip;
std::map<std::array<u_char, 6>, u_int> data_from_mac, data_to_mac;

// MAC 地址转字符串
void mac_to_str(const std::array<u_char, 6> &mac, char *str)
{
    sprintf(str, "%02X-%02X-%02X-%02X-%02X-%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// 获取当前时间字符串
void get_current_time(char *buffer)
{
    time_t rawTime;
    tm *timeInfo;
    time(&rawTime);
    timeInfo = localtime(&rawTime);
    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", timeInfo);
}

std::string user;
std::string password;

int parse_ftp_commands(const std::string &ftp_data)
{
    std::istringstream stream(ftp_data);
    std::string command;

    while (stream >> command)
    {
        if (command == "USER")
        {
            stream >> user;
            std::cout << user << std::endl;
        }
        else if (command == "PASS")
        {
            stream >> password;
            std::cout << password << std::endl;
        }
        else if (command == "230")
        {
            return 1;
        }
        else if (command == "530")
        {
            user.clear();
            password.clear();
            return -1;
        }
    }
    return 0;
}

DWORD lastPrintTime;

// 回调函数：处理每个捕获的包
void packet_handler(u_char *param, const pcap_pkthdr *header, const u_char *pkt_data)
{
    ethernet_header *eth_header = (ethernet_header *)(pkt_data);
    ip_header *ip_header = (struct ip_header *)(pkt_data + 4);
    const char *payload = (char *)(pkt_data + 44);
    char srcMac[18], dstMac[18];
    char srcIp[16], dstIp[16];
    char timestamp[80];

    // 获取时间戳
    get_current_time(timestamp);

    // 解析以太网头
    mac_to_str(eth_header->srcMac, srcMac);
    mac_to_str(eth_header->dstMac, dstMac);


    // 获取源和目标 IP 地址
    inet_ntop(AF_INET, &(ip_header->srcAddr), srcIp, sizeof(srcIp));
    inet_ntop(AF_INET, &(ip_header->dstAddr), dstIp, sizeof(dstIp));

    // 解析 FTP 数据
    std::string ftp_data(payload);
    // std::cout << "Captured FTP Data: " << ftp_data << std::endl;

    // 解析 FTP 命令
    int res = parse_ftp_commands(ftp_data);

    if (res)
    {
        if (res == 1)
        {
            // 记录到 CSV 日志，另外在运行时不要开着excel，会写失败
            fprintf(logfile, "%s,%s,%s,%s,%s,%s,%s,SUCCEED\n", timestamp, srcMac, srcIp, dstMac, dstIp, user.c_str(), password.c_str());
        }
        if (res == -1)
        {
            // 记录到 CSV 日志，另外在运行时不要开着excel，会写失败
            fprintf(logfile, "%s,%s,%s,%s,%s,%s,%s,FAILED\n", timestamp, srcMac, srcIp, dstMac, dstIp, user.c_str(), password.c_str());
        }

        fflush(logfile);
    }

}
int main()
{
    pcap_if_t *interfaces, *temp;
    pcap_t *handle;
    char errorBuffer[PCAP_ERRBUF_SIZE];

    // 获取所有网卡
    pcap_findalldevs(&interfaces, errorBuffer);

    // 显示所有设备
    printf("网卡：\n");
    int i = 0;
    for (temp = interfaces; temp; temp = temp->next)
    {
        printf("#%d: %s - %s\n", ++i, temp->name, temp->description);
    }

    if (interfaces == nullptr)
    {
        printf("没找到网卡\n");
        return -1;
    }

    fflush(stdout);

    temp = interfaces;
    // 不推荐用虚拟网卡
    int targetIndex;
    scanf("%d", &targetIndex);

    for (i = 1; i < targetIndex; i++)
    {
        if (temp->next != nullptr)
        {
            temp = temp->next;
        }
    }

    // 打开接口
    char *selectedDevice = temp->name;
    handle = pcap_open_live(selectedDevice, BUFSIZ, 1, 1000, errorBuffer);
    if (handle == nullptr)
    {
        printf("打不开 %s: %s\n", selectedDevice, errorBuffer);
        pcap_freealldevs(interfaces);
        return -1;
    }

    printf("侦听数据流 %s...\n", selectedDevice);

    fflush(stdout);

    // 设置过滤器
    bpf_program fp;
    const char *filter_exp = "tcp port 21";
    if (pcap_compile(handle, &fp, filter_exp, 0, 0xffffffff) == -1)
    {
        printf("过滤器编译失败: %s\n", pcap_geterr(handle));
        return -1;
    }

    if (pcap_setfilter(handle, &fp) == -1)
    {
        printf("设置过滤器失败: %s\n", pcap_geterr(handle));
        return -1;
    }

    logfile = fopen("logfile.csv", "w");
    pcap_loop(handle, 0, packet_handler, nullptr);

    // 关闭接口
    pcap_close(handle);
    pcap_freealldevs(interfaces);
    fclose(logfile);
    return 0;
}

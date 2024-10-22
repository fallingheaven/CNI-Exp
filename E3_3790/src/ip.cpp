#include <pcap.h>
#include <cstdio>
#include <ctime>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <map>
#include <array>

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

DWORD lastPrintTime;

// 回调函数：处理每个捕获的包
void packet_handler(u_char *param, const pcap_pkthdr *header, const u_char *pkt_data)
{
    ethernet_header *eth_header = (ethernet_header *)(pkt_data);
    ip_header *ip_header = (struct ip_header *)(pkt_data + sizeof(struct ethernet_header));
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

    printf("%s,%s,%s,%s,%s,%d\n", timestamp, srcMac, srcIp, dstMac, dstIp, header->len);
    fflush(stdout);
    // 记录到 CSV 日志，另外在运行时不要开着excel，会写失败
    fprintf(logfile, "%s,%s,%s,%s,%s,%d\n", timestamp, srcMac, srcIp, dstMac, dstIp, header->len);

    // 统计数据长度
    data_from_mac[eth_header->srcMac] += header->len;
    data_to_mac[eth_header->dstMac] += header->len;

    data_from_ip[ip_header->srcAddr] += header->len;
    data_to_ip[ip_header->dstAddr] += header->len;

    // 定时输出来自/发至不同 MAC 和 IP 地址的通信数据长度
    auto currentTime = GetTickCount();
    if (currentTime - lastPrintTime > 10000)
        {
            for (auto [addr, x]: data_from_mac)
            {
                mac_to_str(addr, srcMac);
                printf("来自%s的数据长度为%d\n", srcMac, x);
            }

            for (auto [addr, x]: data_to_mac)
            {
                mac_to_str(addr, dstMac);
                printf("发至%s的数据长度为%d\n", dstMac, x);
            }

            for (auto [addr, x]: data_from_ip)
            {
                inet_ntop(AF_INET, &addr, srcIp, sizeof(srcIp));
                printf("来自%s的数据长度为%d\n", srcIp, x);
            }

            for (auto [addr, x]: data_to_ip)
            {
                inet_ntop(AF_INET, &addr, dstIp, sizeof(dstIp));
                printf("发至%s的数据长度为%d\n", dstIp, x);
            }

            fflush(stdout);

            lastPrintTime = currentTime;
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

    logfile = fopen("logfile.csv", "w");
    pcap_loop(handle, 0, packet_handler, nullptr);

    // 关闭接口
    pcap_close(handle);
    pcap_freealldevs(interfaces);

    return 0;
}

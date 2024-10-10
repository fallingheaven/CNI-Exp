#include <opencv2/opencv.hpp>
#include <vector>
#include <cstring>

class CRC32
{
public:
    CRC32()
    {
        // 初始化 CRC32 表
        for (char32_t i = 0; i < 256; i++)
        {
            char32_t crc = i;
            for (char32_t j = 8; j > 0; j--)
            {
                crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : (crc >> 1);
            }
            table[i] = crc;
        }
    }

    char32_t generate(const vector<uchar>& data)
    {
        char32_t crc = 0xFFFFFFFF;
        for (auto byte : data)
        {
            crc = (crc >> 8) ^ table[(crc & 0xFF) ^ byte];
        }
        return ~crc;
    }

    bool verify(const vector<uchar>& data, char32_t expected_crc)
    {
        return generate(data) == expected_crc;
    }

private:
    char32_t table[256]{};
};

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>

#define forup(i, l, r) for (int i = l; i <= r; i++)
#define fdown(i, l, r) for (int i = r; i >= l; i--)

using namespace cv;
using namespace std;

namespace base64
{
    class Encoder
    {
    private:

    public:
        /// 将数据编码为 Base64
        /// \param input
        /// \return
        string base64_encode(const vector<uchar>& input)
        {
            static const string base64_chars =
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789+/";

            string output;
            // 记录每一组的0/1位
            int group = 0;
            // base编码逻辑
            forup (i, 0, input.size())
            {
                // 处理一组
                if (i != 0 && i % 3 == 0)
                {
                    output.push_back(base64_chars[(group >> 18) & 0x3F]);
                    output.push_back(base64_chars[(group >> 12) & 0x3F]);
                    output.push_back(base64_chars[(group >> 6) & 0x3F]);
                    output.push_back(base64_chars[group & 0x3F]);

                    group = 0;
                }

                if (i < input.size())
                {
                    group = (group << 8) + input[i];
                }
                else // i == input.size()
                {
                    // 补全0
                    if (i % 3 == 1)
                    {
                        group <<= 4;
                        output.push_back(base64_chars[(group >> 18) & 0x3F]);
                        output.push_back(base64_chars[(group >> 12) & 0x3F]);
                    }
                    else if (i % 3 == 2)
                    {
                        group <<= 2;
                        output.push_back(base64_chars[(group >> 18) & 0x3F]);
                        output.push_back(base64_chars[(group >> 12) & 0x3F]);
                        output.push_back(base64_chars[(group >> 6) & 0x3F]);
                    }

                    // 没数据的部分用‘=’填充
                    while (output.size() % 4)
                    {
                        output.push_back('=');
                    }

                    break;
                }
            }

            return output;
        }
    };

}

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <map>

#define forup(i, l, r) for (int i = l; i <= r; i++)
#define fdown(i, l, r) for (int i = r; i >= l; i--)

using namespace cv;
using namespace std;

namespace base64
{
    /// 编码器
    class Decoder
    {
    private:
        // b64 到 int（实质上是获取二进制位）的映射
        map<uchar, int> map_b64;

        void build_map_b64()
        {
            map_b64 = map<uchar, int>();

            static const std::string base64_chars =
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789+/";

            for (int i = 0; i < base64_chars.size(); ++i)
            {
                map_b64[base64_chars[i]] = i;
            }
        }
    public:
        Decoder()
        {
            build_map_b64();
        }

        /// 将输入的string转为base64格式
        /// \param input 输入
        /// \return 输出
        vector<uchar> base64_decode(vector<uchar>& input)
        {
            vector<uchar> output;

            int group = 0;
            // 由于我们的处理，正常来说获取到的input的大小应该是4的倍数
            forup (i, 0, input.size())
            {
                if (input[i] == '=') break;

                // 处理一组
                if (i % 4 == 0)
                {
                    output.push_back(char((group >> 18) & 63));
                    output.push_back(char((group >> 12) & 63));
                    output.push_back(char((group >> 6) & 63));
                    output.push_back(char(group & 63));
                    group = 0;
                }

                if (i < input.size())
                {
                    group = (group << 6) + input[i];
                }
            }

            return output;
        }

        /// base64 数据转为 Mat
        /// \param encoded
        /// \return
        cv::Mat base64_to_mat(vector<uchar>& encoded)
        {
            std::vector<uchar> data = base64_decode(encoded);
            cv::Mat img = cv::imdecode(data, cv::IMREAD_COLOR);
            return img;
        }
    };
}

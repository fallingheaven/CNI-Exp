#include <string>
#include <iostream>
#include <opencv2/opencv.hpp>

#define forup(i, l, r) for (int i = l; i <= r; i++)
#define fdown(i, l, r) for (int i = r; i >= l; i--)

// #defeine后使用qrcodegen库
//#define QRCODEGEN

#ifndef QRCODEGEN
    #define QRENCODE
#endif

using namespace std;
using namespace cv;

void print_progress_bar(int progress, int total, const string& info = string(""), int length = 50)
{
    float percentage = (float) progress / total;
    int filled_length = percentage * length;

    string bar(filled_length, '#');
    bar.append(length - filled_length, '-');
    cout << "\r[" << bar << "] " << int(percentage * 100.0) << "% " << info;
    cout.flush();
}

/// 比较两图片是否一致
/// \param img1
/// \param img2
/// \return
bool are_images_identical(const Mat& img1, const Mat& img2)
{
    if (img1.size() != img2.size() || img1.type() != img2.type())
    {
        return false;
    }

    return norm(img1, img2) != 0;
}

/// 逐字节比较函数
/// \param filename
/// \param data_vector
/// \return 1表示一致，-1表示长度不同，-2表示长度一致但数据不同 -3表示至少一个data为空
int compare_vector_with_vector(const vector<uchar>& data1, const vector<uchar>& data2)
{
#ifdef DEBUG
    cout << "data1: "; for (uchar ch : data1) cout << ch; cout << endl;
        cout << "data2: "; for (uchar ch : data2) cout << ch; cout << endl;
#endif

    if (data1.empty() || data2.empty()) return -3;

    if (data1.size() != data2.size())
    {
#ifdef DEBUG
        cerr << "compare_vector_with_vector：两数据的长度不同" << endl;
#endif
        return -1;
    }

    forup (i, 0, data1.size() - 1)
    {
        if (data1[i] != data2[i])
        {
#ifdef DEBUG
            cerr << "compare_vector_with_vector：两数据下标为" << i << "的数据不同" << endl;
#endif
            return -2;
        }
    }

    return 1;
}

/// 逐字节比较函数
/// \param filename
/// \param data_vector
/// \return
bool compare_file_with_vector(const string& filename, const vector<uint8_t>& data_vector)
{
    ifstream file(filename, ios::binary);
    if (!file)
    {
#ifdef DEBUG
        cerr << "compare_file_with_vector：没这个文件" << filename << endl;
#endif
        return false;
    }

    // 逐字节比较
    for (size_t i = 0; i < data_vector.size(); ++i)
    {
        char file_byte;
        if (!file.get(file_byte))
        {
#ifdef DEBUG
            cerr << "compare_file_with_vector：文件长度与vector长度不一致" << endl;
#endif
            return false; // 文件长度与 vector 长度不一致
        }

        if (static_cast<uint8_t>(file_byte) != data_vector[i])
        {
#ifdef DEBUG
            cerr << "compare_file_with_vector：数据不一致，位置: " << i << " 文件字节: "
                          << static_cast<int>(file_byte)
                          << " vector字节: " << static_cast<int>(data_vector[i]) << endl;
#endif
            return false; // 数据不一致
        }
    }

    // 检查文件是否有多余字节
    if (file.get() != ifstream::traits_type::eof())
    {
#ifdef DEBUG
        cerr << "文件长度比vector长度长" << endl;
#endif
        return false;
    }

    return true;
}

/// 指定路径的文件转换为 vector<uint8_t>（用于生成二维码）
/// \param file_name
/// \return
vector<uint8_t> file_to_vector(const string& file_name)
{
    ifstream file(file_name, ios::binary);
    if (!file.is_open())
    {
#ifdef DEBUG
        cerr << "file_to_vector：没这个文件" <<endl;
#endif
    }

    vector<uchar> res =  vector<uchar>(istreambuf_iterator<char>(file), istreambuf_iterator<char>());

#ifdef DEBUG
    cout << "文件转换到vector数据是否一致：" << compare_file_with_vector(file_name, res) << endl;
#endif
    file.close();
    return res;
}

#ifdef QRCODEGEN
/// 将 QrCode 数据转换为 Mat 数据
    /// \param qr
    /// \param scale
    /// \param offset
    /// \return
    cv::Mat qrCode_to_mat(const qrcodegen::QrCode& qr, int scale = 1, int offset = 5)
    {
        offset *= scale;
        int size = qr.getSize();
        int target_size = size * scale;

        // 为了和ffmpeg适配，size为2的倍数
        int real_size;
        if (target_size % 2)
        {
            real_size = target_size + offset * 2 + 1;
        }
        else
        {
            real_size = target_size + offset * 2;
        }

        cv::Mat mat(real_size, real_size, CV_8UC1, Scalar(255));
        mat.setTo(cv::Scalar(255));

        int block_size = scale;

        forup (y, 0, size - 1)
        {
            forup (x, 0, size - 1)
            {
                if (!qr.getModule(x, y)) continue;

                cv::Rect rect(
                        offset + x * block_size,
                        offset + y * block_size,
                        block_size,
                        block_size
                );

                rectangle(mat, rect, Scalar(0), cv::FILLED);
            }
        }
        return mat;
    }
#else
#ifdef QRENCODE
/// 将 QrCode 数据转换为 Mat 数据
    /// \param qr
    /// \param scale
    /// \param offset
    /// \return
    cv::Mat qrCode_to_mat(const QRcode& qr, int scale = 1, int offset = 5)
    {
        offset *= scale;
        int size = qr.width;
        int target_size = size * scale;

        // 为了和ffmpeg适配，size为2的倍数
        int real_size;
        if (target_size % 2)
        {
            real_size = target_size + offset * 2 + 1;
        }
        else
        {
            real_size = target_size + offset * 2;
        }

        cv::Mat mat(real_size, real_size, CV_8UC1, Scalar(255));
        mat.setTo(cv::Scalar(255));

        int block_size = scale;

        forup (y, 0, size - 1)
        {
            forup (x, 0, size - 1)
            {
                // 是黑色
                if (qr.data[y * size + x] & 1)
                {
                    cv::Rect rect(
                            offset + x * block_size,
                            offset + y * block_size,
                            block_size,
                            block_size
                    );

                    rectangle(mat, rect, Scalar(0), cv::FILLED);
                }
            }
        }
        return mat;
    }
#endif
#endif

/// 转换为灰度图
/// \param input
/// \return
Mat convert_to_gray(Mat& input)
{
    cv::Mat gray;

    if (input.empty())
    {
#ifdef DEBUG
        cerr << "convert_to_gray : 输入为空" <<endl;
#endif
        return gray;
    }

    switch (input.channels())
    {
        case 1:
            // 已经是灰度图像
            return input;
        case 3:
            cvtColor(input, gray, cv::COLOR_BGR2GRAY);
            break;
        case 4:
            cvtColor(input, gray, cv::COLOR_BGRA2GRAY);
            break;
        default:
#ifdef DEBUG
            cerr << "convert_to_gray : 不支持的通道数 " << input.channels() << endl;
#endif
            break;
    }

    return gray;
}

/// 画出检测到的二维码范围（测试用）
/// \param src
/// \param box
void draw_detect_line(Mat& src, const Mat& box)
{
    if (box.total() == 4)
    {
        Point2f corners[4];
        for (int i = 0; i < 4; i++)
        {
            corners[i] = box.at<Point2f>(i);
        }

        // 绘制边界框
        line(src, corners[0], corners[1], Scalar(0, 255, 0), 2);
        line(src, corners[1], corners[2], Scalar(0, 255, 0), 2);
        line(src, corners[2], corners[3], Scalar(0, 255, 0), 2);
        line(src, corners[3], corners[0], Scalar(0, 255, 0), 2);
    }
}

/// 输出文件内容的十六进制
/// \param filename
/// \return
void print_file_hex(const string& filename)
{
    ifstream file(filename, ios::binary | ios::ate);
    if (!file)
    {
        throw runtime_error("read_file：没有文件");
    }

    streamsize size = file.tellg();
    file.seekg(0, ios::beg);

    vector<unsigned char> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
    {
        throw runtime_error("read_file：读入文件失败");
    }

    for (size_t i = 0; i < buffer.size(); ++i) {
        // 每16字节换行
        if (i > 0 && i % 16 == 0) {
            cout << '\n';
        }

        cout << hex << setw(2) << setfill('0') << static_cast<int>(buffer[i]) << ' ';
    }

    // 恢复十进制格式
    cout << dec << '\n';
}

///
/// \param file1
/// \param file2
/// \param outputFilePath
double compare_difference(const string& origin_file, const string& current_file, const string& output_path)
{

//        auto f1 = file_to_vector(origin_file);
//        auto f2 = file_to_vector(current_file);
//        cout << std::format("{}和{}数据是否一致：", origin_file, current_file) << equal(f1.begin(), f1.end(), f2.begin(), f2.end());
    ifstream f1(current_file, ios::binary);
    ifstream f2(origin_file, ios::binary);
    ofstream output(output_path, ios::binary);

    // 检查文件是否成功打开
    if (!f1.is_open() || !f2.is_open() || !output.is_open())
    {
#ifdef DEBUG
        cerr << "compare_difference：无法打开输入文件或输出文件" << endl;
#endif
        return 0;
    }

    char byte1, byte2;
    unsigned char diff = 0;
    size_t diff_count = 0;

    while (f1.get(byte1) && f2.get(byte2))
    {
        diff = ~(byte1 ^ byte2);

        output.write(reinterpret_cast<const char*>(&diff), sizeof(diff));

        diff_count += (8 - bitset<8>(diff).count());
    }

    // 文件一数据更多
    while (f1.get(byte1))
    {
        diff = 0;

        output.write(reinterpret_cast<const char *>(&diff), sizeof(diff));

        diff_count += 8;
    }

    // 文件二数据更多
    while (f2.get(byte2))
    {
        diff = 0;

        output.write(reinterpret_cast<const char *>(&diff), sizeof(diff));

        diff_count += 8;
    }

    // 关闭文件
    f1.close();
    f2.close();
    output.close();

    double file_size = (double)filesystem::file_size(origin_file);
    return 1 - (double)diff_count / 8.0f / file_size;
}
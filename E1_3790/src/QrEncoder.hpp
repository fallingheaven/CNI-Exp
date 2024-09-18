#include <opencv2/objdetect.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <ranges>
#include <filesystem>
#include <fstream>
#include <bitset>
#include <format>
#include <random>

#include "qrcodegen.hpp"
#include "qrencode.h"

#include "ffmpeg_cmd.hpp"
#include "base64_encode.hpp"
#include "base64_decode.hpp"
#include "qrcode_convert.hpp"

#define forup(i, l, r) for (int i = l; i <= r; i++)
#define fdown(i, l, r) for (int i = r; i >= l; i--)

// define后采用b64编码
#define B64_CODE

// define后对编码成的二维码重新解码查看是否一致
#define QRCODE_CHECK

// define后控制台会显示一些数据的结果
#define DEBUG

// #defeine后使用qrcodegen库
//#define QRCODEGEN

#ifndef QRCODEGEN
    #define QRENCODE
#endif

using namespace cv;
using namespace std;
using namespace qrcodegen;

class QrEncoder
{
private:

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

public:
    QrEncoder() = default;

    /// 编码生成二维码图集，随后把二维码合成为视频
    /// \param file_name 输入文件路径
    /// \param output_path 输出路径
    /// \param duration 时长（ms）
    /// \return
    bool encode(string& file_name, string& output_path, int duration, int fps = 10, const string& image_extension = string("jpg"))
    {
        duration /= 1000;

        auto origin = file_to_vector(file_name);
#ifdef B64_CODE
        // b64编码
        base64::Encoder b64_encoder = base64::Encoder();
        vector<uchar> input_file = b64_encoder.base64_encode(origin);

    #ifdef DEBUG
        base64::Decoder b64_decoder = base64::Decoder();
        vector<uchar> before_encode_data = b64_decoder.base64_decode(input_file);
        cout << "编码前的数据：";
        for (uchar ch: before_encode_data) cout << ch; cout <<endl;
    #endif
#else
        vector<uchar>& input_file = origin;
#endif

        int frame_amount = duration * fps;

#ifdef QRCODEGEN
        vector<QrCode> qr_arr;
#else
    #ifdef QRENCODE
        vector<QRcode> qr_arr;
    #endif
#endif
        // 这个库的二维码最多承受1276B，但是太大了opencv识别不出来，悲
        int ch_per_qr = max(1, min((int)ceil(((float)input_file.size() / (float)frame_amount)), 128));
        cout << "每张二维码携带的数据量：" << ch_per_qr * 8 << "B" <<endl;
        // 一个char是8b，一个kb就是128个char
        // 将文件数据分割成多部分生成多个二维码
        for (int i = ch_per_qr - 1; i < input_file.size(); i += ch_per_qr)
        {
#ifndef DEBUG
            print_progress_bar((i - (ch_per_qr - 1)) / ch_per_qr, input_file.size() / ch_per_qr, "二维码编码中");
#endif

            vector<uchar> tmp;
            if (i + ch_per_qr >= input_file.size())
            {
                tmp = vector<uchar>(input_file.begin() + (i - ch_per_qr), input_file.end());
            }
            else
            {
                if (i > ch_per_qr - 1)
                {
                    tmp = vector<uchar>(input_file.begin() + (i - ch_per_qr), input_file.begin() + i);
                }
                else
                {
                    tmp = vector<uchar>(input_file.begin(), input_file.begin() + i);
                }
            }

#ifdef QRCODEGEN
            QrCode qrCode = QrCode::encodeBinary(tmp, QrCode::Ecc::HIGH);
#else
    #ifdef QRENCODE
            QRcode qrCode = *QRcode_encodeData(tmp.size(), tmp.data(), 0, QR_ECLEVEL_H);
    #endif
#endif

#ifdef QRCODE_CHECK
            // 对每个单张二维码进行数据比对
            Mat input_image = qrCode_to_mat(qrCode, 10);

            const vector<uchar>& origin_data = tmp;
            vector<uchar> encoded_data;
            decode(input_image, encoded_data);

#ifdef DEBUG
            cout << std::format("qrCode_{}.{}和原数据是否一致: \n", (i - (ch_per_qr - 1)) / ch_per_qr, image_extension);
#endif
            int compare_res = compare_vector_with_vector(origin_data, encoded_data);
#ifdef DEBUG
            cout << compare_res << endl;
#endif

            if (compare_res == -1 || compare_res == -3)
            {

#ifdef QRCODEGEN
                fill(tmp.begin(), tmp.end(), 0xff);
                qrCode = QrCode::encodeBinary(tmp, QrCode::Ecc::HIGH);
#else
    #ifdef QRENCODE
                // 用另一个二维码库编码
                QrCode another_code = QrCode::encodeBinary(tmp, QrCode::Ecc::HIGH);
                qrCode = QrCode_to_QRcode(another_code);
    #endif
#endif
            }
#endif

            qr_arr.push_back(qrCode);
        }
#ifndef DEBUG
        print_progress_bar(1, 1, "二维码编码完成\n");
#endif

        string qr_path = "qrCodes";
        string work_path = filesystem::current_path().string() + '\\';
        if (filesystem::exists(work_path + qr_path))
        {
            filesystem::remove_all(work_path + qr_path);
        }
        filesystem::create_directory(work_path + qr_path);
        // 由QrCode转换为Mat后，由imwrite写入文件夹
        for (int i = 0; i < qr_arr.size(); i++)
        {
#ifndef DEBUG
            print_progress_bar(i, qr_arr.size() - 1, "二维码绘制中");
#endif

#ifdef QRCODEGEN
            QrCode qrCode = qr_arr[i];
#else
    #ifdef QRENCODE
            QRcode qrCode = qr_arr[i];
    #endif
#endif
//            Mat input_image = qrCode_to_mat(qrCode);
            Mat input_image = qrCode_to_mat(qrCode, 10);

            QRCodeDetector detector = QRCodeDetector();
            while (detector.detectAndDecode(input_image).empty())
            {
                static const string base64_chars =
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz"
                        "0123456789+/";

                random_device rd;
                mt19937 gen(rd()); // 使用 Mersenne Twister 生成器
                uniform_int_distribution<char> dist(1, 63);
#ifdef B64_CODE
                char random_ch = base64_chars[dist(gen)];
#else
                char random_ch = dist(gen);
#endif

                vector<uchar> reconstruct_qrcode = vector<uchar>(ch_per_qr, random_ch);
//                cerr << random_ch <<endl;
#ifdef QRCODEGEN
                qrCode = QrCode::encodeBinary(reconstruct_qrcode, QrCode::Ecc::HIGH);
#else
    #ifdef QRENCODE
                qrCode = *QRcode_encodeData(ch_per_qr, reconstruct_qrcode.data(), 0, QR_ECLEVEL_H);
    #endif
#endif
                input_image = qrCode_to_mat(qrCode, 10);
            }

//            cout << detector.detectAndDecode(input_image) << endl;
//            int size = qrCode.getSize();
//            cv::resize(input_image, input_image, cv::Size(size * 2, size * 2), 0, 0, cv::INTER_CUBIC);

            string img_path = qr_path + std::format("\\qrCode_{}.{}", i, image_extension);

            if (!imwrite(img_path, input_image)) return false;
        }
#ifndef DEBUG
        print_progress_bar(1, 1, "二维码绘制完成\n");
#endif

#ifdef QRCODE_CHECK
//        vector<uchar> encoded_data;
//        for (int i = 0; ; i++)
//        {
//            string img = std::format("\\qrCode_{}.{}", i, image_extension);
//            string img_path = qr_path + img;
//
//            if (!filesystem::exists(img_path)) break;
//
//            cout << "对生成二维码检查：" << img << endl;
//
//            Mat mat = imread(img_path);
//            decode(mat, encoded_data);
//        }
//
//        cout << "编码为二维码前后数据是否一致：" << compare_vector_with_vector(input_file, encoded_data) << endl;
#endif

        ffmpeg::images_to_video(qr_path, output_path, duration);

        // 如果需要检查，要保留文件夹
#ifndef DEBUG
    #ifndef QQRCODE_CHECK
        // 合成视频后，二维码已经没用了
        filesystem::remove_all(qr_path);
    #endif
#endif

        return true;
    }

    /// 解码对应视频，输出文件和解码信息
    /// \param input_video_path 输入文件路径
    /// \param output_file_path 输出文件路径
    /// \param decode_info_path 解码信息输出路径
    /// \return
    bool decode(string& input_video_path, string& output_file_path, string& decode_info_path, const string& origin_file_path = string(""), const string& image_extension = string("jpg"))
    {
        string tmp_frame_folder_path = "tmp_frames";
        string work_path = filesystem::current_path().string() + '\\';
        if (filesystem::exists(work_path + tmp_frame_folder_path))
        {
            filesystem::remove_all(work_path + tmp_frame_folder_path);
        }
        filesystem::create_directory(work_path + tmp_frame_folder_path);

        ffmpeg::video_to_images(input_video_path, tmp_frame_folder_path);

        int fileCount = 0;
        for (const auto& entry : filesystem::directory_iterator(work_path + tmp_frame_folder_path))
        {
            if (filesystem::is_regular_file(entry.status()))
            {
                ++fileCount;
            }
        }

        Mat* previous_img = nullptr;
        vector<uchar> previous_data;
        vector<uchar> encoded_data;
        for (int i = 1; ; i++)
        {
#ifndef DEBUG
            print_progress_bar(i, fileCount, "二维码解码中");
#endif
            string img = std::format("\\frame_{:05d}.{}", i, image_extension);

            string img_path = tmp_frame_folder_path + img;
            if (!filesystem::exists(img_path)) break;

            Mat mat = imread(img_path);
            if (previous_img && are_images_identical(*previous_img, mat)) continue;

            mat = convert_to_gray(mat);
//            cv::threshold(mat, mat, 200, 255, cv::THRESH_BINARY);
            vector<uchar> current_qr_data;
            decode(mat, current_qr_data, previous_data.size());
//            decode(mat, encoded_data, previous_data.size());

            // 数据一致，这里默认是相同的
            if (!previous_data.empty() && compare_vector_with_vector(previous_data, current_qr_data) != 1) continue;

            encoded_data.insert(encoded_data.end(), current_qr_data.begin(), current_qr_data.end());
#ifdef DEBUG
    #ifdef QRCODE_CHECK
//            Mat tmp = imread(std::format("qrCodes\\qrCode_{}.{}", i - 1, image_extension));
//
//            cout << std::format("qrCodes\\qrCode_{}.{}", i - 1, image_extension) << endl;
//
//            vector<uchar> origin_qr_data;
//            decode(tmp, origin_qr_data);
//            vector<uchar> current_qr_data;
//            decode(mat, current_qr_data);
//
//            cout << "视频传输前后二维码数据是否一致：\n" << compare_vector_with_vector(origin_qr_data, current_qr_data) << '\n' << endl;
    #endif
#endif
            previous_img = &mat;
            previous_data = current_qr_data;
        }
#ifndef DEBUG
        print_progress_bar(1, 1, "二维码解码完成\n");
#endif

#ifdef B64_CODE
        // b64解码
        base64::Decoder b64_decoder = base64::Decoder();
        vector<uchar> output_file_data = b64_decoder.base64_decode(encoded_data);
    #ifdef DEBUG
        for (uchar ch: output_file_data) cout << ch; cout <<endl;
    #endif
#else
        vector<uchar>& output_file_data = encoded_data;
#endif
//        auto output_file_data = b64_decoder.base64_decode(encoded_data);

        // 本来想做把文件还原回去的，但是要对传输的数据再包装一层才行，这么一想，为啥不搞zip压缩呢
//        filesystem::path path(output_file_path);
//        string extension = path.extension().string();
//
//        set<std::string> img_extensions = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff", ".svg"};

//        base64::Decoder decoder;

//        if (img_extensions.find(extension) != img_extensions.end())
//        {
//            auto decoded_data = decoder.base64_to_mat(output_file_data);
//
//            if (!imwrite(output_file_path, decoded_data))
//            {
//                cerr << "decode：imwrite失败";
//                return false;
//            }
//        }
//        else
//        {
            ofstream file = ofstream(output_file_path, ios::binary);
            file.write(reinterpret_cast<const char *>(output_file_data.data()), output_file_data.size());

            file.close();
//        }


        if (!origin_file_path.empty())
        {
            double percentage = compare_difference(origin_file_path, output_file_path, decode_info_path) * 100;

//            print_file_hex(decode_info_path);

            cout << fixed << setprecision(2) << "文件传输正确率：" << percentage << '%' << endl;
        }

#ifndef DEBUG
//        filesystem::remove_all(work_path + tmp_frame_folder_path);
#else
//    #ifdef QRCODE_CHECK
//        filesystem::remove_all(work_path + "qrCodes");
//        filesystem::remove_all(work_path + tmp_frame_folder_path);
//    #else
//        filesystem::remove_all(work_path + tmp_frame_folder_path);
//    #endif
#endif

        return true;
    }

    /// 识别二维码，输出得到的数据
    /// \param input_image 输入的带二维码的Mat数据
    /// \param output_data 输出数据
    /// \param length 如果没能识别出来的话，补上的数据长度，一般传入前一个二维码的有效长度
    /// \return
    bool decode(Mat& input_image, vector<uchar>& output_data, int length = 0)
    {
        QRCodeDetector qrDecoder = QRCodeDetector();
        Mat bbox, rectified_image;

        input_image = convert_to_gray(input_image);

        if (!qrDecoder.detect(input_image, bbox)) return false;

        string data = qrDecoder.decode(input_image, bbox, rectified_image);

#ifdef DEBUG
        cout << "识别得到的二维码数据：" << data <<endl;
#endif

        if (data.empty())
        {

            vector<uchar> ch = vector<uchar>(length, 'A');

            output_data.insert(output_data.end(), ch.begin(), ch.end());

            return false;
        }

//        for (char ch : data)
//        {
//            output_data.push_back(ch);
//        }

        output_data.insert(output_data.end(), data.begin(), data.end());

        return true;
    }
};

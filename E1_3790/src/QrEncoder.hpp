#include <opencv2/objdetect.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <ranges>
#include <filesystem>
#include <fstream>
#include <bitset>

#include "qrcodegen.hpp"

#include "ffmpeg_cmd.hpp"

#define forup(i, l, r) for (int i = l; i <= r; i++)
#define fdown(i, l, r) for (int i = r; i >= l; i--)

using namespace cv;
using namespace std;
using namespace qrcodegen;

class QrEncoder
{
private:
    string _input_file;
    string _output_file;

    /// 指定路径的文件转换为 vector<uint8_t>（用于生成二维码）
    /// \param file_name
    /// \return
    vector<uint8_t> file_to_vector(const string& file_name)
    {
        FILE* input_file = fopen(file_name.c_str(), "rb");
        if (!input_file)
        {
            throw runtime_error("file_to_vector：没这个文件");
        }

        fseek(input_file, 0, SEEK_END);
        auto fileSize = ftell(input_file);
        fseek(input_file, 0, SEEK_SET);

        vector<uint8_t> buffer(fileSize);

        if (fread(buffer.data(), 1, fileSize, input_file) != fileSize)
        {
            fclose(input_file);
            throw runtime_error("file_to_vector：文件读入出错");
        }

        fclose(input_file);
        return buffer;
    }

    /// 将 QrCode 数据转换为 Mat 数据
    /// \param qr
    /// \param target_size
    /// \param offset
    /// \return
    cv::Mat qrCode_to_mat(const qrcodegen::QrCode& qr, int target_size = 0, int offset = 5)
    {
        int size = qr.getSize();
        if (target_size == 0) target_size = size;

        int real_size = target_size + offset * 2;

        cv::Mat mat(real_size, real_size, CV_8UC1);
        mat.setTo(cv::Scalar(255));

        float moduleSize = (float)target_size / (float)size;

        forup (y, 0, size - 1)
        {
            forup (x, 0, size - 1)
            {
                Scalar_<double> color = qr.getModule(x, y) ? Scalar(0) : Scalar(255);

                cv::Rect rect(
                        (int)((float)offset + (float)x * moduleSize),
                        (int)((float)offset + (float)y * moduleSize),
                        int(moduleSize),
                        int(moduleSize)
                );

                rectangle(mat, rect, color, cv::FILLED);
            }
        }
        return mat;
    }

    /// 转换为灰度图
    /// \param input
    /// \return
    Mat convert_to_gray(Mat& input)
    {
        cv::Mat gray;

        if (input.empty())
        {
            cerr << "convert_to_gray : 输入为空" <<endl;
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
                cerr << "convert_to_gray : 不支持的通道数 " << input.channels() << endl;
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

    ///
    /// \param file1
    /// \param file2
    /// \param outputFilePath
    void compare_difference(const string& file1, const string& file2, const string& output_path) {
        ifstream f1(file1, ios::binary);
        ifstream f2(file2, ios::binary);
        ofstream output(output_path, ios::binary);

        // 检查文件是否成功打开
        if (!f1.is_open() || !f2.is_open() || !output.is_open()) {
            cerr << "无法打开输入文件或输出文件\n";
            return;
        }

        char byte1, byte2;
        bitset<32> diff = 0;
        bitset<32> b0 = 0, b1 = 1;
        int count = 0;

        while (f1.get(byte1) && f2.get(byte2))
        {
            diff = (diff << 1) | (byte1 == byte2? b1: b0);
            count++;

            if (count % 32 == 0)
            {
                output.write(reinterpret_cast<const char*>(&diff), sizeof(diff));
                count = 0;
            }
        }

        byte1 = '\0';
        byte2 = '\0';

        // 文件一数据更多
        while (f1.get(byte1))
        {
            diff = (diff << 1) | b0;
            count++;

            if (count % 32 == 0)
            {
                output.write(reinterpret_cast<const char *>(&diff), sizeof(diff));
                count = 0;
            }
        }

        if (count % 32 != 0)
        {
            output.write(reinterpret_cast<const char *>(&diff), sizeof(diff));
            count = 0;
        }

        // 文件二数据更多
        while (f2.get(byte2))
        {
            diff = (diff << 1) | b0;
            count++;

            if (count % 32 == 0)
            {
                output.write(reinterpret_cast<const char *>(&diff), sizeof(diff));
                count = 0;
            }
        }

        if (count % 32 != 0)
        {
            output.write(reinterpret_cast<const char *>(&diff), sizeof(diff));
            count = 0;
        }

        // 关闭文件
        f1.close();
        f2.close();
        output.close();
    }

public:
    QrEncoder() = default;

    /// 编码生成二维码图集，随后把二维码合成为视频
    /// \param file_name 输入文件路径
    /// \param output_path 输出路径
    /// \param duration 时长（ms）
    /// \return
    bool encode(string& file_name, string& output_path, double duration, double fps = 60, const string& image_extension = string("jpg"))
    {
        _input_file = file_name;
        auto input_file = file_to_vector(file_name);

        double frame_amount = duration / 1000.0f * fps;
        double kb_per_qr = double(input_file.size()) * 8.0 / frame_amount;

        vector<QrCode> qr_arr;
        int ch_per_qr = int(128 * kb_per_qr);
        // 一个char是8b，一个kb就是128个char
        // 将文件数据分割成多部分生成多个二维码
        for (int i = ch_per_qr - 1; i < input_file.size(); i += ch_per_qr)
        {
            if (i + ch_per_qr >= input_file.size())
            {
                auto tmp = vector<unsigned char>(input_file.begin() + (i - (ch_per_qr - 1)), input_file.begin() + i);
                QrCode qrCode = QrCode::encodeBinary(tmp, QrCode::Ecc::HIGH);
                qr_arr.push_back(qrCode);
            }
        }

        string qr_path = "bin\\qrCodes";
        // 由QrCode转换为Mat后，由imwrite写入文件夹
        for (int i = 0; i < qr_arr.size(); i++)
        {
            QrCode qrCode = qr_arr[i];
            Mat input_image = qrCode_to_mat(qrCode, 300);

            if (!imwrite(qr_path + cv::format("qrCode_{0}.{1}", i, image_extension.c_str()), input_image)) return false;
        }

        ffmpeg::images_to_video(qr_path, output_path);
        return true;
    }

    /// 解码对应视频，输出文件和解码信息
    /// \param input_video_path 输入文件路径
    /// \param output_file_path 输出文件路径
    /// \param decode_info_path 解码信息输出路径
    /// \return
    bool decode(string& input_video_path, string& output_file_path, string& decode_info_path, const string& image_extension = string("jpg"))
    {
        string tmp_frame_folder_path = R"(bin\tmp_frames)";
        filesystem::create_directory(tmp_frame_folder_path);

        ffmpeg::video_to_images(input_video_path, tmp_frame_folder_path);

        vector<uchar> output_file_data;

        for (int i = 0; ; i++)
        {
            char img[256];
            snprintf(img, 256,
                    "\\%05d.%s", i, image_extension.c_str());

            string img_path = tmp_frame_folder_path + img;
            if (!filesystem::exists(img_path)) break;

            Mat mat = imread(img_path);
            decode(mat, output_file_data);
        }


        filesystem::path path(output_file_path);
        string extension = path.extension().string();

        set<std::string> img_extensions = {"jpg", "jpeg", "png", "bmp", "tif", "tiff", "svg"};

        base64::Decoder decoder;

        if (img_extensions.find(extension) != img_extensions.end())
        {
            auto decoded_data = decoder.base64_to_mat(output_file_data);

            if (!imwrite(output_file_path, decoded_data))
            {
                cerr << "decode：imwrite失败";
                return false;
            }
        }
        else
        {
            ofstream file = ofstream(output_file_path, ios::binary);
            file.write(reinterpret_cast<const char *>(output_file_data.data()), output_file_data.size());

            file.close();
        }


        _output_file = output_file_path;

        compare_difference(_input_file, _output_file, decode_info_path);

        filesystem::remove_all(tmp_frame_folder_path);

        return true;
    }

    /// 识别二维码，输出得到的数据
    /// \param input_image 输入的带二维码的Mat数据
    /// \param output_data 输出数据
    /// \return
    bool decode(Mat& input_image, vector<uchar>& output_data)
    {
        QRCodeDetector qrDecoder = QRCodeDetector();
        Mat bbox, rectified_image;

        convert_to_gray(input_image);

        if (!qrDecoder.detect(input_image, bbox)) return false;

        string data = qrDecoder.decode(input_image, bbox, rectified_image);
        output_data.insert(output_data.end(), data.begin(), data.end());

        return true;
    }
};

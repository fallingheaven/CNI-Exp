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
#include "zbar.h"

#include "ffmpeg_cmd.hpp"
#include "base64_encode.hpp"
#include "base64_decode.hpp"
#include "qrcode_convert.hpp"
#include "zlib_compress.hpp"
#include "utility.hpp"

#define forup(i, l, r) for (int i = l; i <= r; i++)
#define fdown(i, l, r) for (int i = r; i >= l; i--)

// define后采用b64编码
//#define B64_CODE

// define后采用zlib压缩
//#define ZLIB

// define后对编码成的二维码重新解码查看是否一致
#define QRCODE_CHECK

// define后控制台会显示一些数据的结果
//#define DEBUG

// #defeine后使用qrcodegen库
//#define QRCODEGEN

#ifndef QRCODEGEN
    #define QRENCODE
#endif

using namespace cv;
using namespace std;
using namespace qrcodegen;
using namespace zbar;

class QrEncoder
{
private:
    // 二维码储存的数据
    struct QrData
    {
        int index;
        int len; // 含有的数据长度，或者说应该有的长度
        vector<uchar> data;

        bool start;
        bool end;

        QrData()
        {
            index = 0;
            len = 0;
            start = false;
            end = false;
        }
    };

    void debug_print_qrData(const QrData& qr_data)
    {
#ifdef DEBUG
        cout << "index: " << qr_data.index << '\n'
             << "len: " << qr_data.len << '\n'
             << "start: " << (int)qr_data.start << '\n'
             << "end: " << (int)qr_data.end << '\n';
        for (uchar ch : qr_data.data) cout << (int)ch << ' '; cout << endl;
#endif
    }

    vector<uchar> serialize(const QrData& qrData)
    {
        vector<uchar> serializedData;

        for (int i = 3; i >= 0; --i)
        {
            serializedData.push_back((qrData.index >> (i * 8)) & 0xFF); // 从高位字节到低位
        }

        for (int i = 3; i >= 0; --i)
        {
            serializedData.push_back((qrData.len >> (i * 8)) & 0xFF); // 从高位字节到低位
        }

        serializedData.push_back(qrData.start ? '1' : '0');

        serializedData.push_back(qrData.end ? '1' : '0');

        serializedData.insert(serializedData.end(), qrData.data.begin(), qrData.data.end());

        return serializedData;
    }

    QrData deserialize(const vector<uchar>& serializedData)
    {
        QrData qrData;
        size_t offset = 0;

        for (int i = 0; i < 4; ++i)
        {
            qrData.index = (qrData.index << 8) | serializedData[offset++];
        }

        for (int i = 0; i < 4; ++i)
        {
            qrData.len = (qrData.len << 8) | serializedData[offset++];
        }

        qrData.start = serializedData[offset] == '1';
        offset++;

        qrData.end = serializedData[offset] == '1';
        offset++;

        qrData.data = vector<uchar>(serializedData.begin() + offset, serializedData.end());

        return qrData;
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

        vector<uchar>& input_file = origin;

        int frame_amount = duration * fps;

#ifdef QRCODEGEN
        vector<QrCode> qr_arr;
#else
    #ifdef QRENCODE
        vector<QRcode> qr_arr;
    #endif
#endif
        // 太大了opencv识别不出来，悲
        int ch_per_qr = max(1, min((int)ceil(((float)input_file.size() / (float)frame_amount)), 512));
        cout << "每张二维码携带的数据量：" << ch_per_qr * 8 << "B" <<endl;
        // 一个char是8b，一个kb就是128个char
        // 将文件数据分割成多部分生成多个二维码
        for (int i = ch_per_qr - 1; i < input_file.size(); i += ch_per_qr)
        {
#ifndef DEBUG
            print_progress_bar((i - (ch_per_qr - 1)) / ch_per_qr, input_file.size() / ch_per_qr, "二维码编码中");
#endif

            QrData tmp = QrData();
            tmp.index = (i - (ch_per_qr - 1)) / ch_per_qr + 1;
            if (i + ch_per_qr >= input_file.size())
            {
                tmp.data = vector<uchar>(input_file.begin() + (i - ch_per_qr), input_file.end());
                tmp.end = 1;
            }
            else
            {
                tmp.data = vector<uchar>(input_file.begin() + (i - ch_per_qr + 1), input_file.begin() + i + 1);
                if (i == ch_per_qr - 1) tmp.start = 1;
            }

            tmp.len = tmp.data.size();

            vector<uchar> tmp_string = serialize(tmp);

            QrData deserialized_qr = deserialize(tmp_string);
            debug_print_qrData(deserialized_qr);

#ifdef QRCODEGEN
            vector<uchar> string_to_vector = vector<uchar>(tmp_string.begin(), tmp_string.end());
            QrCode qrCode = QrCode::encodeBinary(string_to_vector, QrCode::Ecc::HIGH);
#else
    #ifdef QRENCODE
            QRcode qrCode = *QRcode_encodeData(tmp_string.size(),
                                               reinterpret_cast<const unsigned char *>(tmp_string.data()), 0, QR_ECLEVEL_H);
    #endif
#endif

            Mat input_image = qrCode_to_mat(qrCode, 10);
            vector<uchar> data;
            decode(input_image, data);

            QrData qr_data = deserialize(data);
            debug_print_qrData(qr_data);

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

            vector<uchar> data;
            decode(input_image, data);

            QrData qr_data = deserialize(data);
            debug_print_qrData(qr_data);

            string img_path = qr_path + std::format("\\qrCode_{}.{}", i + 1, image_extension);

            if (!imwrite(img_path, input_image)) return false;
        }
#ifndef DEBUG
        print_progress_bar(1, 1, "二维码绘制完成\n");
#endif

        ffmpeg::images_to_video(qr_path, output_path, duration);

        // 如果需要检查，要保留文件夹
#ifndef DEBUG
        // 合成视频后，二维码已经没用了
        filesystem::remove_all(qr_path);
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

        int file_count = 0;
        for (const auto& entry : filesystem::directory_iterator(work_path + tmp_frame_folder_path))
        {
            if (filesystem::is_regular_file(entry.status()))
            {
                ++file_count;
            }
        }

        // 这里把每一张二维码的结果都单独存放在vector中
        Mat* previous_img = nullptr;
        QrData previous_data;
        vector<QrData> encoded_data;
        bool data_start = false;
        for (int i = 1; i <= file_count; i++)
        {
#ifndef DEBUG
            print_progress_bar(i, file_count, "二维码解码中");
#endif
            string img = std::format("\\frame_{:05d}.{}", i, image_extension);
#ifdef DEBUG
            cout << img << endl;
#endif

            string img_path = tmp_frame_folder_path + img;
            if (!filesystem::exists(img_path)) break;

            Mat mat = imread(img_path);
            if (previous_img && are_images_identical(*previous_img, mat)) continue;

            mat = convert_to_gray(mat);

            vector<uchar> current_qr_data_string;
            decode(mat, current_qr_data_string, previous_data.data.size());

            if (current_qr_data_string.empty()) continue;
            QrData current_qr_data = deserialize(current_qr_data_string);
//            decode(mat, encoded_data, previous_data.size());

            debug_print_qrData(current_qr_data);

            // 数据接收开始
            if (!data_start)
            {
                if (current_qr_data.start)
                {
                    data_start = true;
                } else
                {
                    continue;
                }
            }

            // 二维码数据序号一样，重复
            if (!previous_data.data.empty() && previous_data.index == current_qr_data.index) continue;

            // 有中间二维码没识别出来
            if (!previous_data.data.empty() && previous_data.index + 1 < current_qr_data.index)
            {
                forup (k, 1, current_qr_data.index - previous_data.index - 1)
                {
                    QrData recovery_qrcode = QrData();
                    recovery_qrcode.index = previous_data.index + k;
                    recovery_qrcode.data = vector<uchar>(previous_data.len, 'a');
                    encoded_data.push_back(recovery_qrcode);
                }
            }

            encoded_data.push_back(current_qr_data);
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

            // 数据接收结束
            if (current_qr_data.end) break;
        }


#ifndef DEBUG
        print_progress_bar(1, 1, "二维码解码完成\n");
#endif



//        vector<uchar> compressed_data;
        string complete_data;
        for (auto data_block : encoded_data)
        {
            complete_data.insert(complete_data.end(), data_block.data.begin(), data_block.data.end());
        }



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
            file.write(reinterpret_cast<const char *>(complete_data.data()), complete_data.size());

            file.close();
//        }


        if (!origin_file_path.empty())
        {
            double percentage = compare_difference(origin_file_path, output_file_path, decode_info_path) * 100;

//            print_file_hex(decode_info_path);

            cout << fixed << setprecision(2) << "文件传输正确率：" << percentage << '%' << endl;
        }

#ifndef DEBUG
        filesystem::remove_all(work_path + tmp_frame_folder_path);
#endif

        return true;
    }

    /// 识别二维码，输出得到的数据
    /// \param input_image 输入的带二维码的Mat数据
    /// \param output_data 输出数据
    /// \param length 如果没能识别出来的话，补上的数据长度，一般传入前一个二维码的有效长度
    /// \return
    bool decode(Mat input_image, vector<uchar>& output_data, int length = 0)
    {
//        QRCodeDetector qrDecoder = QRCodeDetector();
//        Mat bbox, rectified_image;
//
//        input_image = convert_to_gray(input_image);
//
//        if (!qrDecoder.detect(input_image, bbox)) return false;
//
//        string data = qrDecoder.decode(input_image, bbox, rectified_image);

        ImageScanner scanner;
        scanner.set_config(ZBAR_QRCODE, ZBAR_CFG_ENABLE, 1);
        Image zbar_image(input_image.cols, input_image.rows, "Y800", input_image.data, input_image.cols * input_image.rows);
        int res = scanner.scan(zbar_image);
        if (res == 0) return false;

        string data;

        for (Image::SymbolIterator symbol = zbar_image.symbol_begin();
             symbol != zbar_image.symbol_end();
             ++symbol)
        {
            data += symbol->get_data();
        }

#ifdef DEBUG
        cout << "decode: 识别得到的二维码数据：" << data <<endl;
#endif

//        output_data += data;
        output_data.insert(output_data.end(), data.begin(), data.end());

        return true;
    }
};

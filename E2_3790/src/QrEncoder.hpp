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

#include <codecvt>

#include "qrcodegen.hpp"
#include "qrencode.h"
#include "zbar.h"

#include "ffmpeg_cmd.hpp"
#include "base64_encode.hpp"
#include "base64_decode.hpp"
#include "qrcode_convert.hpp"
#include "utility.hpp"
#include "crc32.hpp"

#define forup(i, l, r) for (int i = l; i <= r; i++)
#define fdown(i, l, r) for (int i = r; i >= l; i--)
#define fs filesystem

// define后采用b64编码
//#define B64_CODE

// define后采用zlib压缩
//#define ZLIB

// define后对编码成的二维码重新解码查看是否一致
#define QRCODE_CHECK

// define后控制台会显示一些数据的结果
//#define DEBUG

// define后使用qt的libqrencode库
#define QRENCODE


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
        vector<uchar> data;
        int len;
        bool start;
        bool end;

        QrData()
        {
            index = 0;
            start = false;
            end = false;
        }

        QrData(vector<uchar>& serializedData)
        {
            deserialize(serializedData);
        }

        void deserialize(const vector<uchar>& serializedData)
        {
            size_t offset = 0;

            for (int i = 0; i < 4; ++i)
            {
                index = (index << 8) | serializedData[offset++];
            }

            for (int i = 0; i < 4; ++i)
            {
                len = (len << 8) | serializedData[offset++];
            }

            start = serializedData[offset++] == '1';

            end = serializedData[offset++] == '1';

            data = vector<uchar>(serializedData.begin() + offset, serializedData.end());
        }
    };

    /// 帧格式
    struct DataFrame
    {
        uint8_t begin{};
        uint8_t destination{};
        uint8_t source{};
        uint16_t length;
        vector<uchar> data;
        uint32_t crc{};

        DataFrame() = default;

        DataFrame(vector<uchar>& data, uint8_t source, uint8_t destination)
        {
            this->data = data;

            this->source = source;
            this->destination = destination;
            this->length = this->data.size();
            // 规定begin为0x7F，之所以前一个是7，是因为第一个位如果是1，转成char后就是负数，int类型下也是负数了
            this->begin = (char)0x7F;
        }

        DataFrame(vector<uchar>& serializedData)
        {
            deserialize(serializedData);
        }

        ///
        /// \param serializedData
        /// \return 返回1成功，返回-1表示length和data中有一个不对的
        int deserialize(const vector<uchar>& serializedData)
        {
            size_t offset = 0;

            begin = serializedData[offset++];
            destination = serializedData[offset++];
            source = serializedData[offset++];

            for (int i = 1; i <= 2; ++i)
            {
                length = (length << 8) | serializedData[offset++];
            }

            for (int i = 0; i < length; i++)
            {
                data.push_back(serializedData[offset++]);
                if (offset >= serializedData.size())
                {
                    return -1;
                }
            }

            for (int i = 1; i <= 4; ++i)
            {
                crc = (crc << 8) | serializedData[offset++];
            }

            return 1;
        }

        uint32_t generate_crc32()
        {
            CRC32 generator = CRC32();
            crc = generator.generate(data);
            return crc;
        }

        bool verify_crc32()
        {
            CRC32 verifier = CRC32();
            return verifier.verify(data, crc);
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

    vector<uchar> serialize(const DataFrame& data)
    {
        vector<uchar> serializedData;

        serializedData.push_back(data.begin);
        serializedData.push_back(data.destination);
        serializedData.push_back(data.source);

        for (int i = 1; i >= 0; i--)
            serializedData.push_back((data.length >> (i * 8)) & 0xFF);

        serializedData.insert(serializedData.end(), data.data.begin(), data.data.end());

        for (int i = 3; i >= 0; i--)
            serializedData.push_back((data.crc >> (i * 8)) & 0xFF);

        return serializedData;
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

    ///
    /// \param output_file_path
    /// \param output_data
    void append_data(const string& output_file_path, vector<uchar>& output_data)
    {
        ofstream file = ofstream(output_file_path, ios::binary | ios::app);
        file.write(reinterpret_cast<const char *>(output_data.data()), output_data.size());

        file.close();
    }

    ///
    /// \param uchar_vec
    /// \param end_idx
    /// \param data_len
    /// \param ch_per_qr
    /// \return
    vector<uchar> uchar_to_qrcode(vector<uchar>& uchar_vec, int end_idx, int ch_per_qr)
    {
        QrData tmp = QrData();
        tmp.index = (end_idx - (ch_per_qr - 1)) / ch_per_qr + 1;

        if (end_idx >= uchar_vec.size()) return {};
        else if (end_idx + ch_per_qr >= uchar_vec.size())
        {
            tmp.data = vector<uchar>(uchar_vec.begin() + (end_idx - ch_per_qr), uchar_vec.end());
            tmp.end = true;
        }
        else
        {
            tmp.data = vector<uchar>(uchar_vec.begin() + (end_idx - ch_per_qr + 1), uchar_vec.begin() + end_idx + 1);
            if (end_idx == ch_per_qr - 1) tmp.start = true;
        }

        tmp.len = tmp.data.size();

        vector<uchar> serialized_qr_data = serialize(tmp);

        QrData deserialized_qr = QrData(serialized_qr_data);
//        debug_print_qrData(deserialized_qr);

        return serialized_qr_data;
    }

    ///
    /// \param folder_name
    static void create_folder_of_work_folder(const string& folder_name)
    {
        string work_path = filesystem::current_path().string() + '\\';
        if (filesystem::exists(work_path + folder_name))
        {
            filesystem::remove_all(work_path + folder_name);
        }
        filesystem::create_directory(work_path + folder_name);
    }

public:
    QrEncoder() = default;

    /// 编码生成二维码图集，随后把二维码合成为视频
    /// \param input_folder 输入文件路径
    /// \param output_path 输出路径
    /// \param duration 时长（ms）
    /// \param max_trans_unit 最大传输单元（字节）
    /// \return
    bool encode(string& input_folder, string& output_path, int duration, int max_trans_unit,
                int fps = 10, const string& image_extension = string("jpg"))
    {
        // 毫秒转成秒
        duration /= 1000;

        // 读入目标文件夹中所有bin文件（的路径）
        vector<fs::path> input_files;
        for (const auto& entry : fs::directory_iterator(input_folder))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".bin")
            {
                input_files.push_back(entry.path());
            }
        }

        // 将文件夹中的文件映射到对应的数据上
        size_t total_size = 0;
        map<fs::path, vector<uchar>> input_file_vectors;
        for (auto& file : input_files)
        {
            input_file_vectors[file] = file_to_vector(file.string());
            total_size += input_file_vectors[file].size();
        }

        int frame_amount = duration * fps;
        // 要生成的二维码
        vector<QRcode> qr_arr;
        // 二维码能携带的数据量是有限的，并且还要根据用户输入的帧大小进行限制
        int ch_per_qr = max(
            1,
            min(
                (int)ceil(((float)total_size / (float)frame_amount)),
                min(512, max_trans_unit - 9)
                )
            );
        // 一个char是8b，一个kb就是128个char
        cout << "每张二维码携带的数据量：" << ch_per_qr * 8 << "B" <<endl;
        int current_data_size = 0;
        // 分时复用
        for (int t = 0, i = ch_per_qr - 1; ; t++, i += ch_per_qr)
        {
#ifndef DEBUG
            print_progress_bar(current_data_size, total_size, "二维码编码中");
#endif
            // 是否所有的数据都处理完毕
            bool flag = false;
            for (auto& [file_path, file_data] : input_file_vectors)
            {
                // 生成载荷部分
                vector<uchar> qr_data = uchar_to_qrcode(input_file_vectors[file_path], i, ch_per_qr);
                if (qr_data.empty()) continue;
                current_data_size += qr_data.size() - 10;

                flag = true;

                // 生成帧
                DataFrame data_frame = DataFrame(qr_data, (uint8_t)stoi(file_path.stem().string()), 0);
                data_frame.generate_crc32();

                vector<uchar> serialized_frame = serialize(data_frame);
                // 为什么要用base64编码？
                // 因为zbar检测二维码是按照utf-8编码读数据的，所以如果最高位是1，就会变成c2/c3开头的宽字符，为了避免，我们使用base64，即四个6位bit表示3个char
                base64::Encoder b64_encoder = base64::Encoder();
                serialized_frame = b64_encoder.base64_encode(serialized_frame);

                QRcode qrCode = *QRcode_encodeData((int)serialized_frame.size(), serialized_frame.data(), 0, QR_ECLEVEL_H);

                qr_arr.push_back(qrCode);

                // 测试识别二维码得到的帧数据是否一致
#ifdef DEBUG
                Mat input_image = qrCode_to_mat(qrCode, 10);
                vector<uchar> tmp;
                decode(input_image, tmp);
                base64::Decoder b64_decoder = base64::Decoder();
                serialized_frame = b64_decoder.base64_decode(serialized_frame);
                tmp = b64_decoder.base64_decode(tmp);
                cout << "编码前字符串: "; for (auto ch : serialized_frame) cout << (int)ch << ' '; cout <<endl;
                cout << "解码后字符串: "; for (auto ch : tmp) cout << (int)ch << ' '; cout <<endl;
                DataFrame frame = DataFrame(tmp);
                cout << "二维码解码后" << hex << frame.crc << endl;
#endif
            }

            if (!flag) break;
        }

#ifndef DEBUG
        print_progress_bar(1, 1, "二维码编码完成\n");
#endif

        // 创建存放二维码的临时文件夹
        string qr_path = "qrCodes";
        create_folder_of_work_folder(qr_path);

        // 由QrCode转换为Mat后，由imwrite写入文件夹
        for (int i = 0; i < qr_arr.size(); i++)
        {
#ifndef DEBUG
            print_progress_bar(i, qr_arr.size() - 1, "二维码绘制中");
#endif
            QRcode qrCode = qr_arr[i];

            Mat input_image = qrCode_to_mat(qrCode, 10);

            vector<uchar> data;
            decode(input_image, data);

#ifdef DEBUG
            for (auto ch : data) cout << ch << ' '; cout << endl;
#endif

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
    bool decode(string& input_video_path, string& output_info_directory, string& origin_file_path, const string& image_extension = string("jpg"))
    {
        string tmp_frame_folder = "tmp_frames";
        create_folder_of_work_folder(tmp_frame_folder);
        create_folder_of_work_folder(output_info_directory);

        if (origin_file_path.empty()) origin_file_path = output_info_directory;

        ffmpeg::video_to_images(input_video_path, tmp_frame_folder);

        int file_count = 0;
        for (const auto& entry : filesystem::directory_iterator(tmp_frame_folder))
        {
            if (filesystem::is_regular_file(entry.status()))
            {
                ++file_count;
            }
        }

        // 这里把每一张二维码的结果都单独存放在vector中
        Mat* previous_img = nullptr;
        QrData encoded_data;
        map<uint8_t, QrData> previous_data;
        set<uint8_t> data_start;
        for (int i = 1; i <= file_count; i++)
        {
#ifndef DEBUG
            print_progress_bar(i, file_count, "二维码解码中");
#endif
            string img = std::format("\\frame_{:05d}.{}", i, image_extension);
#ifdef DEBUG
            cout << img << endl;
#endif

            string img_path = tmp_frame_folder + img;
            if (!filesystem::exists(img_path)) break;

            Mat mat = imread(img_path);
            // 重帧
            if (previous_img && are_images_identical(*previous_img, mat)) continue;

            mat = convert_to_gray(mat);

            // 解码得帧数据
            vector<uchar> current_frame_data_string;
            decode(mat, current_frame_data_string);

            // 没收到数据
            if (current_frame_data_string.empty()) continue;

            base64::Decoder b64_decoder = base64::Decoder();
            current_frame_data_string = b64_decoder.base64_decode(current_frame_data_string);
            DataFrame current_frame_data = DataFrame(current_frame_data_string);

            // crc检验有误
            if (!current_frame_data.verify_crc32()) continue;
            // 长度和数据的大小中有一个不一样
            if (current_frame_data.length != current_frame_data.data.size()) continue;

            QrData current_qr_data = QrData(current_frame_data.data);

//            debug_print_qrData(current_qr_data);

            // 数据接收开始
            if (!data_start.contains(current_frame_data.source))
            {
                if (current_qr_data.start)
                {
                    data_start.insert(current_frame_data.source);
                } else
                {
                    continue;
                }
            }

            // 二维码数据序号一样，重复
            if (!previous_data[current_frame_data.source].data.empty() &&
                 previous_data[current_frame_data.source].index == current_qr_data.index) continue;

            // 有中间二维码没识别出来
            if (!previous_data[current_frame_data.source].data.empty() &&
                 previous_data[current_frame_data.source].index + 1 < current_qr_data.index)
            {
                forup (k, 1, current_qr_data.index - previous_data[current_frame_data.source].index - 1)
                {
                    QrData recovery_qrcode = QrData();
                    recovery_qrcode.index = previous_data[current_frame_data.source].index + k;
                    recovery_qrcode.data = vector<uchar>(previous_data[current_frame_data.source].len, 'a');
                    append_data(output_info_directory + std::format("{:d}.bin", (int)current_frame_data.source), recovery_qrcode.data);
                }
            }

            encoded_data = current_qr_data;

            append_data(output_info_directory + std::format("/{:d}.bin", (int)current_frame_data.source), encoded_data.data);

            previous_img = &mat;
            previous_data[current_frame_data.source] = current_qr_data;

            // 数据接收结束
            if (current_qr_data.end) break;
        }


#ifndef DEBUG
        print_progress_bar(1, 1, "二维码解码完成\n");
#endif

        for (char source : data_start)
        {
            double percentage = compare_difference(origin_file_path + std::format("/{:d}.bin", (int)source), output_info_directory + std::format("/{:d}.bin", (int)source),
                                                   output_info_directory + std::format("/{:d}.val", (int)source)) * 100;

            cout << fixed << setprecision(2) << std::format("{:d}.bin", (int)source) << "文件传输正确率：" << percentage << '%' << endl;
        }


#ifndef DEBUG
        filesystem::remove_all(tmp_frame_folder);
#endif

        return true;
    }

    /// 识别二维码，输出得到的数据
    /// \param input_image 输入的带二维码的Mat数据
    /// \param output_data 输出数据
    /// \param length 如果没能识别出来的话，补上的数据长度，一般传入前一个二维码的有效长度
    /// \return
    static bool decode(const Mat& input_image, vector<uchar>& output_data)
    {
        ImageScanner scanner;
        scanner.set_config(ZBAR_QRCODE, ZBAR_CFG_ENABLE, 1);
        Image zbar_image(input_image.cols, input_image.rows, "Y800", input_image.data, input_image.cols * input_image.rows);
        int res = scanner.scan(zbar_image);
        if (res == 0) return false;

        vector<unsigned char> data;

        for (Image::SymbolIterator symbol = zbar_image.symbol_begin(); symbol != zbar_image.symbol_end(); ++symbol)
        {
            const string& symbolData = symbol->get_data();

            for (unsigned char ch : symbolData)
            {
                data.push_back(ch);
            }
        }

#ifdef DEBUG
//        cout << "decode: 识别得到的二维码数据：";
//        for (char ch : data) cout << hex << (uint32_t)ch << ' '; cout << endl;
#endif

        output_data.insert(output_data.end(), data.begin(), data.end());

        return true;
    }
};

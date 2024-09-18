//#include "base64_encode.hpp"
//#include "base64_decode.hpp"
//#include "ffmpeg_cmd.hpp"
#include "QrEncoder.hpp"
#include <iostream>
#include <string>
#include <map>

bool encode_input(int argc, char** argv);
bool decode_input(int argc, char** argv);
map<string, function<bool(int argc, char** argv)>> input_func =
{
        { "encode", encode_input },
        { "decode", decode_input }
};

// argv[0]默认是exe文件的路径
int main(int argc, char** argv)
{
    string command = argv[1];
    if (input_func.find(command) != input_func.end())
    {
        if (!input_func[command](argc, argv))
        {
            return -1;
        }
        else
        {
            return 1;
        }
    }

    return 0;
}

bool encode_input(int argc, char** argv)
{
    system("chcp 65001");
    // 指令格式：encode <输入文件路径> <输出文件路径> <生成视频时长>
    if (argc < 3) return false;

    string input_file_path = argv[2];
    string output_file_path = argv[3];
    int video_length = stoi(argv[4]);

    QrEncoder encoder = QrEncoder();
    if (!encoder.encode(input_file_path, output_file_path, video_length)) return false;

    return true;
}

bool decode_input(int argc, char** argv)
{
    // 指令格式：decode <输入文件路径> <输出文件路径> <解码信息输出路径> (<原文件的路径>，用于比较解码准确性)
    if (argc < 5) return false;

    string input_file_path = argv[2];
    string output_file_path = argv[3];
    string output_info_path = argv[4];

    string origin_file_path = argv[5];

    QrEncoder encoder = QrEncoder();
    if (argc == 5)
    {
        if (!encoder.decode(input_file_path, output_file_path, output_file_path)) return false;
    }
    else if (argc == 6)
    {
        if (!encoder.decode(input_file_path, output_file_path, output_info_path, origin_file_path)) return false;
    }

    return true;
}
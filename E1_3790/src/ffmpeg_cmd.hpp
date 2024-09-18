#include <iostream>
#include <string>

namespace ffmpeg
{
    using namespace std;

    /// 对应文件夹中的图片合并为视频
    /// \param image_folder_path 目标文件夹
    /// \param output_path 视频输出路径（带文件）
    /// \param fps 帧率
    /// \param image_extension 目标图片格式后缀
    void images_to_video(string& image_folder_path, string& output_path,
                         int duration, int fps = 10,
                         const string& image_extension = string("jpg"))
    {
        string cmd =
            std::format("ffmpeg -loglevel error -framerate {0} -i {1}\\qrCode_%d.{2} -c:v libx264 -t {3} -r {4} -pix_fmt yuv420p {5}",
                       fps, image_folder_path, image_extension, duration, fps, output_path);

//        cout<<cmd<<endl;

        system(cmd.data());
    }

    /// 对应视频分解为图像
    /// \param video_folder_path 目标视频路径
    /// \param output_path 输出文件夹
    /// \param fps 帧率
    /// \param image_extension 输出图片格式后缀
    void video_to_images(string& video_folder_path, string& output_path,
                         const string& image_extension = string("jpg"))
    {
        string cmd =
            std::format("ffmpeg -loglevel error -i {0} {1}\\frame_%05d.{2}",
                       video_folder_path, output_path, image_extension);

//        cout<<cmd<<endl;

        system(cmd.data());
    }
}
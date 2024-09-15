#include <iostream>

namespace ffmpeg
{
    /// 对应文件夹中的图片合并为视频
    /// \param image_folder_path 目标文件夹
    /// \param output_path 视频输出路径（带文件）
    /// \param fps 帧率
    /// \param image_extension 目标图片格式后缀
    void images_to_video(string& image_folder_path, string& output_path,
                         int duration, int fps = 60,
                         const string& image_extension = string("jpg"))
    {
        string cmd =
            cv::format("ffmpeg -framerate {0} -i {1}\\qrCode_%%d.{2} -c:v libx264 -t {3} -r {4} -pix_fmt yuv720p {5}",
                       fps, image_folder_path.c_str(), image_extension.c_str(), duration, fps, output_path.c_str());

        system(cmd.data());
    }

    /// 对应视频分解为图像
    /// \param video_folder_path 目标视频路径
    /// \param output_path 输出文件夹
    /// \param fps 帧率
    /// \param image_extension 输出图片格式后缀
    void video_to_images(string& video_folder_path, string& output_path,
                         int fps = 60,
                         const string& image_extension = string("jpg"))
    {
        string cmd =
            cv::format(R"(ffmpeg -framerate {0} -i {1} -vf "fps={2}" {3}\frame_%%05d.{4})",
                       fps, video_folder_path.c_str(), fps, output_path.c_str(), image_extension.c_str());

        system(cmd.c_str());
    }
}
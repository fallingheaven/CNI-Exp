#include <opencv2/objdetect.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
#include "qrcodegen.hpp"

#define forup(i, l, r) for (int i = l; i <= r; i++)
#define fdown(i, l, r) for (int i = r; i >= l; i--)

using namespace cv;
using namespace std;
using namespace qrcodegen;

vector<uint8_t> file_to_vector(const std::string& filename)
{
    FILE* input_file = fopen(filename.c_str(), "rb");
    if (!input_file)
    {
        throw runtime_error("Failed to open file");
    }

    fseek(input_file, 0, SEEK_END);
    auto fileSize = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    vector<uint8_t> buffer(fileSize);

    if (fread(buffer.data(), 1, fileSize, input_file) != fileSize)
    {
        fclose(input_file);
        throw runtime_error("Failed to read file");
    }

    fclose(input_file);
    return buffer;
}

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
            cerr << "convert_to_gray : 不支持的通道数 " << input.channels() << std::endl;
            break;
    }

    return gray;
}

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

int main(int argc, char* argv[])
{
    auto input_file = file_to_vector("data.txt");

    QrCode qrCode = QrCode::encodeBinary(input_file, QrCode::Ecc::HIGH);

    Mat input_image = qrCode_to_mat(qrCode, 200);

//    Mat input_image = imread("qrcode.jpg");

    QRCodeDetector qrDecoder = QRCodeDetector();
    Mat bbox, rectified_image;

    convert_to_gray(input_image);
//    imshow("input_qr_code", input_image);

    std::string data = qrDecoder.detectAndDecode(input_image, bbox, rectified_image);

    draw_detect_line(input_image, bbox);

    if(data.length() > 0)
    {
        cout << "Decoded Data : " << data << endl;

        rectified_image.convertTo(rectified_image, CV_8UC3);
        imshow("Rectified QRCode", rectified_image);

        waitKey(0);
    }
    else
        cout << "QR Code not detected" << endl;

    waitKey(0);

    return 0;
}

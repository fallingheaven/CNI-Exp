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

Mat get_rectified_qrcode(Mat& input_image, Mat& bbox)
{
    if (bbox.total() == 4)
    {
        Point2f corners[4];
        for (int i = 0; i < 4; i++)
        {
            corners[i] = bbox.at<Point2f>(i);
        }

        int size = 200;
        Point2f targetCorners[4] =
        {
                Point2f(0, 0),
                Point2f(size, 0),
                Point2f(size, size),
                Point2f(0, size)
        };

        Mat perspectiveMatrix = getPerspectiveTransform(corners, targetCorners);

        Mat output_image;
        warpPerspective(input_image, output_image, perspectiveMatrix, Size(size, size));

        return output_image;
    }
    return {};
}

double calculate_snr(const Mat& original, const Mat& noisy)
{
    Mat noise = noisy - original;

    Scalar signal_mean = mean(original);
    Scalar noise_stddev;
    meanStdDev(noise, Scalar(), noise_stddev);

    double snr = signal_mean[0] / noise_stddev[0];

    return snr;
}

double compute_ssim(const Mat& img1, const Mat& img2) {
    const double C1 = 6.5025, C2 = 58.5225;

    // 将图像转换为浮点型
    Mat img1f, img2f;
    img1.convertTo(img1f, CV_32F);
    img2.convertTo(img2f, CV_32F);

    // 计算均值
    Mat mu1, mu2;
    GaussianBlur(img1f, mu1, Size(11, 11), 1.5);
    GaussianBlur(img2f, mu2, Size(11, 11), 1.5);

    Mat mu1_sq = mu1.mul(mu1);
    Mat mu2_sq = mu2.mul(mu2);
    Mat mu1_mu2 = mu1.mul(mu2);

    // 计算方差和协方差
    Mat sigma1_sq, sigma2_sq, sigma12;
    GaussianBlur(img1f.mul(img1f), sigma1_sq, Size(11, 11), 1.5);
    sigma1_sq -= mu1_sq;

    GaussianBlur(img2f.mul(img2f), sigma2_sq, Size(11, 11), 1.5);
    sigma2_sq -= mu2_sq;

    GaussianBlur(img1f.mul(img2f), sigma12, Size(11, 11), 1.5);
    sigma12 -= mu1_mu2;

    // 计算 SSIM
    Mat t1 = 2 * mu1_mu2 + C1;
    Mat t2 = 2 * sigma12 + C2;
    Mat t3 = t1.mul(t2);

    t1 = mu1_sq + mu2_sq + C1;
    t2 = sigma1_sq + sigma2_sq + C2;
    t1 = t1.mul(t2);

    Mat ssim_map;
    divide(t3, t1, ssim_map);

    Scalar mssim = mean(ssim_map); // 返回 SSIM 的平均值

    return mssim[0]; // 由于是灰度图，返回第一个通道的值
}

int main(int argc, char* argv[])
{
    Mat input_image = imread("qrCode_5.jpg");

    QRCodeDetector qrDecoder = QRCodeDetector();
    Mat bbox, rectified_image;

    string data;
    qrDecoder.detect(input_image, bbox);

    Mat m1 = get_rectified_qrcode(input_image, bbox);
//    imshow("qrcode", m1);


    input_image = imread("frame_00034.jpg");

    qrDecoder = QRCodeDetector();

    qrDecoder.detect(input_image, bbox);

    Mat m2 = get_rectified_qrcode(input_image, bbox);
//    imshow("qrcode", m2);

    resize(m1, m1, m2.size());
    cout << calculate_snr(m1, m2) << endl;
    cout << compute_ssim(m1, m2) << endl;

    imshow("m1", m1);
    imshow("m2", m2);

    waitKey();

//    if(data.length() > 0)
//    {
//        cout << "Decoded Data : " << data << endl;
//
//        rectified_image.convertTo(rectified_image, CV_8UC3);
////        imshow("Rectified QRCode", rectified_image);
//
//        waitKey(0);
//    }
//    else
//        cout << "QR Code not detected" << endl;
//
//    waitKey(0);

    return 0;
}

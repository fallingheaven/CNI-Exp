#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;

bool are_images_identical(const Mat& img1, const Mat& img2)
{
    if (img1.size() != img2.size() || img1.type() != img2.type())
    {
        return false;
    }

    return countNonZero(img1 != img2) == 0;
}

bool are_image_data_identical(const Mat& img1, const Mat& img2)
{
    QRCodeDetector detector = QRCodeDetector();
    string data1 = detector.detectAndDecode(img1);
    string data2 = detector.detectAndDecode(img2);

    cout << data1 << '\n' << data2 << endl;
    return equal(data1.begin(), data1.end(), data2.begin(), data2.end());
}

int main() {
    Mat img1 = imread("qrCode_8.jpg", IMREAD_UNCHANGED);
    Mat img2 = imread("frame_00009.jpg", IMREAD_UNCHANGED);

    if (are_images_identical(img1, img2))
    {
        cout << "The images are identical." << endl;
    }
    else
    {
        cout << "The images are not identical." << endl;
    }

    if (are_image_data_identical(img1, img2))
    {
        cout << "The images' data are identical." << endl;
    }
    else
    {
        cout << "The images' data are not identical." << endl;
    }

    return 0;
}

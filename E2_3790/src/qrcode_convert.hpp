#include <opencv2/opencv.hpp>
#include "qrcodegen.hpp"
#include <qrencode.h>
#include <string>

using namespace std;
using namespace qrcodegen;

QRcode QrCode_to_QRcode(const QrCode& qr)
{
    int size = qr.getSize();
    vector<bool> data(size * size);

    QRcode code = QRcode(0, size);
    code.data = new uchar[size * size];

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            code.data[y * size + x] = qr.getModule(x, y)? 1: 0;
        }
    }

    return code;
}
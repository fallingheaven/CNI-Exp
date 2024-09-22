#include <zlib.h>
#include <cstdio>
#include <cstring>

int compress_data(Bytef *src, const uLongf src_len, const Bytef *dest, uLongf* dest_len) {
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    deflateInit(&stream, Z_DEFAULT_COMPRESSION);

    stream.next_in = (Bytef *)src;
    stream.avail_in = src_len;
    stream.next_out = (Bytef *)dest;
    stream.avail_out = *dest_len;

    int res = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);

    *dest_len = stream.total_out;
    return res;
}


int uncompress_with_recovery(Bytef *src, const uLongf src_len, const Bytef *dest, uLongf* dest_len) {
    z_stream stream = {nullptr};
    stream.next_in = (Bytef *)src;
    stream.avail_in = src_len;
    stream.next_out = (Bytef *)dest;
    stream.avail_out = *dest_len;

    int res = inflateInit(&stream);
    if (res != Z_OK)
    {
        return res;
    }

    do
    {
        res = inflate(&stream, Z_SYNC_FLUSH);
        if (res == Z_DATA_ERROR)
        {
            // 遇到损坏的数据，跳过损坏部分
            cout << "数据错误，尝试跳过损坏部分继续解压..." << endl;
            stream.next_in++; // 跳过当前字节，继续
            stream.avail_in--;
        }
        else if (res == Z_STREAM_END)
        {
            *dest_len = stream.total_out;
            inflateEnd(&stream);
            return Z_OK;
        }
    } while (stream.avail_in > 0);

    inflateEnd(&stream);
    return Z_DATA_ERROR;
}
#include "ffmpegs.h"
#include <QDebug>
#include <QFile>

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#define ERR_BUF \
    char errbuf[1024]; \
    av_strerror(ret, errbuf, sizeof (errbuf));

#define END(func) \
    if (ret < 0) { \
        ERR_BUF; \
        qDebug() << #func << "error" << errbuf; \
        goto end; \
    }

ffmpegs::ffmpegs()
{

}

void ffmpegs::convertRawVideo(RawVideoFrame &in,
                            RawVideoFrame &out) {

    //上下文
    SwsContext *ctx = nullptr;

    // 输入、输出缓冲区（指向每一个平面的数据）类似音频重采样planer
    uint8_t *inData[4], *outData[4];

    // 每一个平面一行的大小
    int inStrides[4], outStrides[4];

    // 每一帧图片的大小
    int inFrameSize, outFrameSize;
    // 返回结果
    int ret = 0;

    // 创建上下文
    ctx = sws_getContext(in.width,in.height,in.format,
                         out.width,out.height,out.format,
                         SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!ctx) {
        qDebug() << "sws_getContext error";
        goto end;
    }

    // 输入缓冲区
    ret = av_image_alloc(inData,inStrides,in.width,in.height,in.format,1);

    END(av_image_alloc);

    // 输出缓冲区
    ret = av_image_alloc(outData, outStrides,out.width, out.height, out.format, 1);

    END(av_image_alloc);


    //计算每一帧的大小
    inFrameSize = av_image_get_buffer_size(in.format,in.width,in.height,1);
    outFrameSize = av_image_get_buffer_size(out.format, out.width, out.height, 1);

    // 拷贝输入数据
    memcpy(inData[0],in.pixels,inFrameSize);

    //转换
    sws_scale(ctx,inData,inStrides,0,in.height,outData,outStrides);

    // 写到输出文件去
    out.pixels = (char *)malloc(outFrameSize);
    memcpy(out.pixels,outData[0],outFrameSize);


end:

    av_freep(&inData[0]);
    av_freep(&outData[0]);
    sws_freeContext(ctx);

}

// yuv420p
//    inData[0] = (uint8_t *) malloc(in.frameSize); 指向y
//    inData[1] = inData[0] + 所有Y的大小; //指向u
//    inData[2] = inData[0] + 所有Y的大小 + 所有U的大小; //指向v
// 每一个平面一行的大小 假设图片是像素640*480.那么y就是640列*480行，所以y平面一行就是640，inStrides[0] = 640。而yuv420p，水平分量和竖直分量都是2:1，所以u/v都是320列*240行，一行是320，inStrides[1] = 320
//    inStrides[0] = 640; // Y
//    inStrides[1] = 320; // U
//    inStrides[2] = 320; // V

/*
640*480，yuv420p

---- 640个Y -----
YY............YY |
YY............YY |
YY............YY |
YY............YY
................ 480行
YY............YY
YY............YY |
YY............YY |
YY............YY |
YY............YY |

---- 320个U -----
UU............UU |
UU............UU |
UU............UU |
UU............UU
................ 240行
UU............UU
UU............UU |
UU............UU |
UU............UU |
UU............UU |

---- 320个V -----
VV............VV |
VV............VV |
VV............VV |
VV............VV
................ 240行
VV............VV
VV............VV |
VV............VV |
VV............VV |
VV............VV |

600*600，rgb24
//    inStrides[0] = 1800; // RGB 600*3
//    inStrides[1] = 0;
//    inStrides[2] = 0;
-------  600个RGB ------
RGB RGB .... RGB RGB  |
RGB RGB .... RGB RGB  |
RGB RGB .... RGB RGB
RGB RGB .... RGB RGB 600行
RGB RGB .... RGB RGB
RGB RGB .... RGB RGB  |
RGB RGB .... RGB RGB  |
RGB RGB .... RGB RGB  |

6 * 4，yuv420p

YYYYYY
YYYYYY
YYYYYY
YYYYYY

UUU
UUU

VVV
VVV
*/


void ffmpegs::convertRawVideo(RawVideoFile &in,
                              RawVideoFile &out) {

    //上下文
    SwsContext *ctx = nullptr;

    // 输入、输出缓冲区（指向每一个平面的数据）类似音频重采样planer
    uint8_t *inData[4], *outData[4];

    // 每一个平面一行的大小
    int inStrides[4], outStrides[4];

    // 每一帧图片的大小
    int inFrameSize, outFrameSize;
    // 返回结果
    int ret = 0;
    // 进行到了那一帧
    int frameIdx = 0;
    // 文件
    QFile inFile(in.fileName), outFile(out.fileName);

    // 创建上下文
    ctx = sws_getContext(in.width,in.height,in.format,
                         out.width,out.height,out.format,
                         SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!ctx) {
        qDebug() << "sws_getContext error";
        goto end;
    }

    // 输入缓冲区（这个方法就是告诉inData[]每个平面是怎么指向的）
    ret = av_image_alloc(inData,inStrides,in.width,in.height,in.format,1);

    END(av_image_alloc);

    // 输出缓冲区
    ret = av_image_alloc(outData, outStrides,out.width, out.height, out.format, 1);

    END(av_image_alloc);

    //打开文件
    if (!inFile.open(QFile::ReadOnly)) {
        qDebug() << "file open error" << in.fileName;
        goto end;
    }
    if (!outFile.open(QFile::WriteOnly)) {
        qDebug() << "file open error" << out.fileName;
        goto end;
    }

    //计算每一帧的大小
    inFrameSize = av_image_get_buffer_size(in.format,in.width,in.height,1);
    outFrameSize = av_image_get_buffer_size(out.format, out.width, out.height, 1);

    // 进行每一帧的转换
    while (inFile.read((char *)inData[0],inFrameSize) == inFrameSize) {

        sws_scale(ctx,inData,inStrides,0,in.height,outData,outStrides);

        //写入文件
        outFile.write((char *)outData[0],outFrameSize);

        qDebug() << "转换完第" << frameIdx++ << "帧";
    }

end:

    inFile.close();
    outFile.close();
    av_freep(&inData[0]);
    av_freep(&outData[0]);
    sws_freeContext(ctx);
}

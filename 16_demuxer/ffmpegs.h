#ifndef FFMPEGS_H
#define FFMPEGS_H

#include <QObject>

extern "C" {
#include <libavformat/avformat.h>
}

//typedef struct {

//    const char* fileName;
//    int width;
//    int height;
//    AVPixelFormat pixelFormat;
//    int fps;

//}VideoDecodeSpec;

typedef struct {
    const char *filename;
    int width;
    int height;
    AVPixelFormat pixFmt;
    int fps;
} VideoDecodeSpec;

class FFmpegs
{
public:
    FFmpegs();

//    static void h264Decode(VideoDecodeSpec &out,
//                          const char *inFilename);
                           static void h264Decode(const char *inFilename,
                                                      VideoDecodeSpec &out);
};

#endif // FFMPEGS_H

#ifndef FFMPEGS_H
#define FFMPEGS_H

extern "C" {
#include <libavutil/avutil.h>
}

typedef struct {

    const char *fileName;
    int width;
    int height;
    AVPixelFormat format;

}RawVideoFile;


typedef struct {

    char *pixels;
    int width;
    int height;
    AVPixelFormat format;

}RawVideoFrame;

class ffmpegs
{
public:
    ffmpegs();

    static void convertRawVideo(RawVideoFrame &in,
                                RawVideoFrame &out);
    static void convertRawVideo(RawVideoFile &in,
                                RawVideoFile &out);
};

#endif // FFMPEGS_H

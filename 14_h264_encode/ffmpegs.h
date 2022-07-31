#ifndef FFMPEGS_H
#define FFMPEGS_H

#include <QObject>

extern "C" {
#include <libavformat/avformat.h>
}

typedef struct {

    const char* fileName;
    int width;
    int height;
    AVPixelFormat pixFormat;
    int fps;

}VideoEncodeSpec;

class FFmpegs
{
public:
    FFmpegs();

    static void h264Encode(VideoEncodeSpec &in,
                          const char *outFilename);


};

#endif // FFMPEGS_H

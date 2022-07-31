#ifndef FFMPEGS_H
#define FFMPEGS_H

#include <QObject>

extern "C" {
#include <libavformat/avformat.h>
}

typedef struct {

    const char* fileName;
    int64_t ch_layout;
    AVSampleFormat sampleFormat;
    int sampleRate;

}AudioEncodeSpec;

class FFmpegs
{
public:
    FFmpegs();

    static void aacEncode(AudioEncodeSpec &in,
                          const char *outFilename);


};

#endif // FFMPEGS_H

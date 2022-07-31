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

}AudioDecodeSpec;

class FFmpegs
{
public:
    FFmpegs();

    static void aacDecode(AudioDecodeSpec &out,
                          const char *inFilename);
};

#endif // FFMPEGS_H

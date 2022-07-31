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

}ResampleAudioSpec;

class FFmpegs
{
public:
    FFmpegs();

    static void audioResample(ResampleAudioSpec &in,
                              ResampleAudioSpec &out);

    static void audioResample(const char* infileName,
                              int64_t in_ch_layout,
                              AVSampleFormat informat,
                              int insampleRate,
                              const char* outfileName,
                              int64_t out_ch_layout,
                              AVSampleFormat outformat,
                              int outsampleRate
                              );
};

#endif // FFMPEGS_H

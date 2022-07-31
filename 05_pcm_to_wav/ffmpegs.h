#ifndef FFMPEGS_H
#define FFMPEGS_H
#include <QObject>

#define AUDIOFORMATPCM 1
#define AUDIOFORMATFLOAT 3
// WAV文件头（44字节）
typedef struct WAVHead {
    //RIFFChunk
    char riffChunkID[4] = {'R','I','F','F'};
    //char riffChunkID2[4] = "RIFF";//Initializer-string for char array is too long
    //"RIFF" = {'R','I','F','F'，'\0'};在C中是5个字节 包含\0
    uint32_t riffChunkSize;
    char riffFormat[4] = {'W','A','V','E'};

    //'fmt' sub-chunk
    char fmtChunkID[4] = {'f','m','t',' '};
    // fmt chunk的data大小：存储PCM数据时，是16
    uint32_t fmtChunkSize = 16;
    // 音频编码，1表示PCM，3表示Floating Point(f32le就是Floating Point)
    uint16_t audioFormat = AUDIOFORMATPCM; //小端模式：01 00 (PCM)
    // 声道数
    uint16_t numChannels;
    // 采样率
    uint32_t sampleRate;
    // 字节率 = sampleRate * blockAlign
    uint32_t byteRate;
    // 一个样本的字节数 = bitsPerSample * numChannels >> 3
    uint16_t blockAlign;
    // 位深度
    uint16_t bitsPerSample;

    //'data' sub-chunk
    char dataChunkID[4] = {'d','a','t','a'};
    // data chunk的data大小：音频数据的总长度，即文件总长度减去文件头的长度(一般是44)
    uint32_t dataChunkSize;

}WAVHead;

class FFMpegs
{
public:
    FFMpegs();
    static void pcm2wav(WAVHead &head,
                        const char *pcmfileName,
                        const char *wavfileName);
};

#endif // FFMPEGS_H

#include "playthread.h"
#include <QDebug>
#include <SDL2/SDL.h>
#include <QFile>

#define FILENAME "/Users/tonytaoliang/Downloads/in.pcm"
// 采样率
#define SAMPLE_RATE 44100
#define SAMPLE_FORMAT AUDIO_S16LSB
// 采样大小/位深度(AUDIO_S16LSB最后2位就是代表位深度)
//#define SAMPLE_SIZE (SAMPLE_FORMAT & 0xFF) //有经验的话0xFF应该有一个mask的 #define SDL_AUDIO_MASK_BITSIZE       (0xFF)
#define SAMPLE_SIZE SDL_AUDIO_BITSIZE(SAMPLE_FORMAT)
// 声道数
#define CHANNELS 2
// SDL音频缓冲区的样本数量
#define SAMPLES 1024
// 每个样本占多少字节 右移就是除以8
//#define BYTES_PER_SAMPLE ((SAMPLE_SIZE*CHANNELS) / 8)
#define BYTES_PER_SAMPLE ((SAMPLE_SIZE*CHANNELS) >> 3)
// 读取PCM文件缓冲区的大小
#define BUFFER_SIZE (SAMPLES*BYTES_PER_SAMPLE)

playThread::playThread(QObject *parent)
    : QThread{parent}
{
    connect(this,&playThread::finished,this,&playThread::deleteLater);
}

playThread::~playThread(){
    disconnect();
    requestInterruption();
    quit();
    wait();
    qDebug() << this << "析构函数";
}

void showSDLVersion() {

    SDL_version version;
    SDL_VERSION(&version);
    qDebug() << version.major << version.minor << version.patch;
}

//char *buffData;//用来接收每次文件读取的数据
//int bufferLen;//用来接收每次文件读取的长度

struct AudioBuffer
{
    char *buffData = nullptr;//用来接收每次文件读取的数据
    int bufferLen = 0;//用来接收每次文件读取的长度
    int pullLen = 0;//每次填充的实际大小(主要用于最后一次还没播放完毕就close了。计算时间用)

};

//一调用SDL_OpenAudio就会自动等待音频设备回调(会回调多次)
//spec.callback : SDL_AudioCallback callback
//SDL_AudioCallback: typedef void (SDLCALL * SDL_AudioCallback) (void *userdata, Uint8 * stream,
//int len); spec.callback是一个函数指针，有的类似定义block

void pull_audio_data(void *userdata,
                     Uint8 * stream,// 需要往stream中填充PCM数据
                     int len){// 希望填充的大小(samples * format * channels / 8) (format * channels = 每个样本多少bit)

    // 清空stream（静音处理）
    SDL_memset(stream,0,len);

    AudioBuffer *buffer = (AudioBuffer*)userdata;
    //文件还没准备好
    if(buffer->bufferLen <= 0) {
        return;
    }

    // 情况1: 如果pcm读取pcm的buffsize比sdl设置的sample大小或者说len小，那么会卡卡的 因为每次只填充一部分
    //     【(填充2048) xx空白2048 xx】【(填充2048) xx空白2048 xx】
    // 情况2:取len、bufferLen的最小值（为了保证数据安全，防止指针越界。当设置的从PCM读取的buffData的长度比此方法的len大时，会塞不下，硬塞就越界了）；同时如果当最后一次读取只有100字节时，不比较最小值，那么就是继续塞4096，最后面就有一些垃圾值
    // PS：因此最好是动态根据SDL设置的样本大小，声道数，样本数量来计算该从PCM中读取的buffsize
    buffer->pullLen = (len > buffer->bufferLen) ? buffer->bufferLen : len;

    //填充数据
    SDL_MixAudio(stream,(Uint8*)buffer->buffData,buffer->pullLen,SDL_MIX_MAXVOLUME);

    buffer->buffData += buffer->pullLen;//+= 挪动指针 每次从文件读取后面的的位置开始填充[第一块，第二块，第三块。。。。 ]

    buffer->bufferLen -= buffer->pullLen;//每次读取的数据已经填充完毕（消费掉从pcm读出的部分data），文件开始继续读取
}

/*
SDL播放音频有2种模式：
Push（推）：【程序】主动推送数据给【音频设备】
Pull（拉）：【音频设备】主动向【程序】拉取数据 通过回调的方式
*/
void playThread::run() {

    // 初始化Audio子系统
    //Returns 0 on success or a negative error code on failure; call SDL_GetError() for more information.
    if(SDL_Init(SDL_INIT_AUDIO)){
        qDebug() << "初始化SDLAudio失败" << SDL_GetError();
        return;
    }

    // 音频参数
    SDL_AudioSpec spec;
    // 采样率
    spec.freq = SAMPLE_RATE;
    // 采样格式（s16le）in.pcm
    spec.format = SAMPLE_FORMAT;
    // 声道数
    spec.channels = CHANNELS;
    // 音频缓冲区的样本数量（这个值必须是2的幂）
    spec.samples = SAMPLES;
    // 回调
    spec.callback = pull_audio_data;
    AudioBuffer buffer;
    // 自定义传递给回调的数据（这里把读取的PCM数据和长度传进去）
    spec.userdata = &buffer;
    // 打开设备
    if(SDL_OpenAudio(&spec,nullptr)){
        qDebug() << "打开设备失败" << SDL_GetError();
        SDL_Quit();
        return;
    }

    QFile file = QFile(FILENAME);

    if(!file.open(QFile::ReadOnly)){

        qDebug() << "file open error" << FILENAME;
        // 关闭设备
        SDL_CloseAudio();
        // 清除所有的子系统
        SDL_Quit();
        return;
    }

    // 开始播放（0是取消暂停）很关键这句代码
    SDL_PauseAudio(0);

    // 存放从文件中读取的数据
    char data[BUFFER_SIZE];

    while (!isInterruptionRequested()) {

        // 只要从文件中读取的音频数据，还没有填充完毕，就跳过.取代下面的while循环
        if(buffer.bufferLen > 0) continue;//

        buffer.bufferLen = file.read(data,BUFFER_SIZE);

        //文件数据全部读取完毕
        if(buffer.bufferLen <= 0){

            //buffer.pullLen防止最后一次还没有播放完毕 直接break，下面就直接close了
            //最后一次需要播放的样本数量
            int samples = buffer.pullLen / BYTES_PER_SAMPLE;
            //最后一次需要播放的时间毫秒(样本数量/采样率 = 时间)
            int ms = samples * 1000 / SAMPLE_RATE;
            SDL_Delay(ms);//等到最后播放完毕才
            break;
        }

        // 读取到了文件数据
        buffer.buffData = data;
        // 等待音频数据填充完毕
        // 只要音频数据还没有填充完毕，就Delay(sleep)
        //        while (bufferLen > 0) {
        //            SDL_Delay(1);
        //        }

    }


    file.close();
    //关闭设备
    SDL_CloseAudio();
    //清除子系统
    SDL_Quit();
}

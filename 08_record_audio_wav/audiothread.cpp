#include "audiothread.h"

#include <QDebug>
#include <QFile>
#include <QDateTime>
#include "ffmpegs.h"

extern "C" {

#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

}

#ifdef Q_OS_MAC
#define fmtName "avfoundation"
#else
#define fmtName "dshow"
#endif

AudioThread::AudioThread(QObject *parent)
    : QThread{parent}
{
    //线程结束后，就提前销毁它，避免占用内存（_audioThread = new AudioThread(this);它之前是跟随window的生命周期的）
    connect(this,&AudioThread::finished,this,&AudioThread::deleteLater);
}

AudioThread::~AudioThread(){

    //内存回收之前，正常结束线程（正在录，突然点击xx销毁窗口的情况）
    requestInterruption();
    //安全退出
    quit();
    wait();
    qDebug() << this << "析构函数";

}

void AudioThread::setStop(bool stop) {

    _stop = stop;

}

//通过上下文来获取录音参数
void showSpec(AVFormatContext *ctx) {
    qDebug() << "开始获取采样参数";
    //获取输入流：AVStream **streams; 2颗星说明数组中装的是AVStream *类型。参考怎么用指针表示数组
    AVStream *stream = ctx->streams[0];
    //获取音频参数
    AVCodecParameters *par = stream->codecpar;
    //声道数
    qDebug() << par->channels;
    //采样率
    qDebug() << par->sample_rate;
    //采样格式(系统原因获得是-1，因此采用下面的方法)
    qDebug() << par->format;
    //每一个样本的一个声道占多少字节(强转为枚举)
    qDebug() << av_get_bytes_per_sample((AVSampleFormat)par->format);

    //编码ID
    qDebug() << par->codec_id;
    //每一个样本的一个声道占多少位
    qDebug() << av_get_bits_per_sample(par->codec_id);
}

//当线程启动时（start）会自动调用run方法
void AudioThread::run() {

    //注册设备：只执行一次，在main.cpp中执行即可
    //avdevice_register_all();

    //2.获取输入格式对象

    const AVInputFormat *fmt = av_find_input_format(fmtName);
    if (!fmt) {
        // 如果找不到输入格式
        qDebug() << "找不到输入格式" << fmtName;
        return;
    }

    //3.打开设备
    //格式上下文（后面通过格式上下文操作设备）
    AVFormatContext *ctx = nullptr;
    /*ffmpeg -list_devices true -f AVFoundation -i dummy
[AVFoundation indev @ 0x7f886fd0eec0] AVFoundation video devices:
[AVFoundation indev @ 0x7f886fd0eec0] [0] FaceTime HD Camera
[AVFoundation indev @ 0x7f886fd0eec0] [1] Capture screen 0
[AVFoundation indev @ 0x7f886fd0eec0] AVFoundation audio devices:
[AVFoundation indev @ 0x7f886fd0eec0] [0] Built-in Microphone*/

    const char *deviceName = ":0";//Built-in Microphone

    int ret = avformat_open_input(&ctx,deviceName,fmt,nullptr);

    //打开设备失败
    if(ret != 0 ){
        //传一个字符串数组
        char errorbuff[1024] = {0};
        av_strerror(ret,errorbuff,sizeof(errorbuff));
        qDebug() << "打开设备失败" << errorbuff;
        return;
    }
//    showSpec(ctx);
    //4.采集数据
    //创建一个文件，后面将采集到的数据写入文件
    QString filePath = "/Users/tonytaoliang/Downloads/";
    filePath += QDateTime::currentDateTime().toString("MM_dd_HH_mm_ss");
    filePath += ".wav";

    QFile file = QFile(filePath);

    if(!file.open(QIODeviceBase::WriteOnly)) {

        //打开文件失败
        qDebug() << "文件打开失败" << filePath;
        // 关闭设备
        avformat_close_input(&ctx);
        return;
    }

    //写入wav头
    AVStream *stream = ctx->streams[0];
    //获取音频参数
    AVCodecParameters *par = stream->codecpar;

    WAVHead head;
    head.numChannels = par->channels;
    head.sampleRate = par->sample_rate;
    head.bitsPerSample = av_get_bits_per_sample(par->codec_id);
    if(par->codec_id >= AV_CODEC_ID_PCM_F32BE){
        head.audioFormat = AUDIOFORMATFLOAT;
    }
    head.blockAlign = head.bitsPerSample*head.numChannels >> 3;
    head.byteRate = head.sampleRate * head.blockAlign;

    file.write((const char*)&head,sizeof(WAVHead));

    qDebug() << head.numChannels << head.sampleRate << head.bitsPerSample
             << head.audioFormat;

    AVPacket packet;

    //下面这种方式可知道错误码；当线程调用requestInterruption()时，isInterruptionRequested就会变为true
    while (!isInterruptionRequested()) {

        ret = av_read_frame(ctx, &packet);

        if (ret == 0) { // 读取成功
//            qDebug() << "我在写入文件" << ret;
            // 将数据写入文件
            file.write((const char*)packet.data,packet.size);
            head.dataChunkSize += packet.size;

            //计算时间
            unsigned long long time = 1000.0 * head.dataChunkSize / head.byteRate;
            emit recordTime(time);

        } else if (ret == AVERROR(EAGAIN)) { // 资源临时不可用#define EAGAIN   35 /* Resource temporarily unavailable */
            continue;
        } else { // 其他错误
            char errbuf[1024] = {0};
            av_strerror(ret, errbuf, sizeof (errbuf));
            qDebug() << "av_read_frame error" << errbuf << ret;
            break;

        }
    }

    //PCM写入完成后修改wav头，注意用seek偏移，不然是直接拼接到最后面[wavhead][pcm][head.dataChunkSize][head.riffChunkSize]
    //正常应该是[wavhead：head.riffChunkSize。。。。。。head.dataChunkSize][pcm]
    //写入dataChunkSize，pcm录完才有这个大小
    file.seek(sizeof(WAVHead) - sizeof(head.dataChunkSize));
    file.write((const char*)&head.dataChunkSize,sizeof (head.dataChunkSize));


    //写入riffChunkSize
    //PCM数据大小 + 44字节的head - riffChunkID - riffChunkSize
    head.riffChunkSize = head.dataChunkSize
                               + sizeof (WAVHead)
                               - sizeof (head.riffChunkID)
                               - sizeof (head.riffChunkSize);

    file.seek(sizeof(head.riffChunkID));
    file.write((const char*)&head.riffChunkSize,sizeof (head.riffChunkSize));

    //5.释放资源
    file.close();

    avformat_close_input(&ctx);

    qDebug() << "线程正常结束";
}

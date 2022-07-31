#include "audiothread.h"
#include <QDebug>
#include <QObject>
#include "demuxer.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/channel_layout.h>
}

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

//当线程启动时（start）会自动调用run方法
void AudioThread::run() {

    VideoDecodeSpec vout;
    vout.filename = "/Users/tonytaoliang/Downloads/ballD.yuv";

    AudioDecodeSpec aout;
    aout.filename = "/Users/tonytaoliang/Downloads/ballD.pcm";

    demuxer().demux("/Users/tonytaoliang/Downloads/ball640.mp4",aout,vout);

    qDebug() << aout.sampleRate
                 << av_get_channel_layout_nb_channels(aout.chLayout)
                 << av_get_sample_fmt_name(aout.sampleFmt);
}

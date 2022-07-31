#include "audiothread.h"
#include <QDebug>
#include <QObject>
#include "ffmpegs.h"

extern "C" {
#include <libavutil/imgutils.h>
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

    VideoDecodeSpec out;
    out.filename = "/Users/tonytaoliang/Downloads/ballD.yuv";

//    FFmpegs::h264Decode("/Users/tonytaoliang/Downloads/ball.h264",out);
FFmpegs::h264Decode("/Users/tonytaoliang/Downloads/ball.h264",out);

    qDebug() << out.width;
    qDebug() << out.height;
    qDebug() << out.fps;
    qDebug() << av_get_pix_fmt_name(out.pixFmt);

}

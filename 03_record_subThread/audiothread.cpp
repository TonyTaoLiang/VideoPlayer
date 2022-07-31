#include "audiothread.h"
#include <QDebug>
#include <QFile>

extern "C" {

    #include <libavdevice/avdevice.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
}

#ifdef Q_OS_MAC

    #define FMTNAME "avfoundation"

#else

    #define FMTNAME "dshow"

#endif

AudioThread::AudioThread(QObject *parent)
    : QThread{parent}
{
    //线程一结束，就清除它的内存
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

void AudioThread::run() {

    //第二步：获取输入格式对象
    const AVInputFormat *fmt = av_find_input_format(FMTNAME);

    if(!fmt) {

        qDebug() << "找不到输入格式";

        return;
    }

    //第三步：打开设备
    AVFormatContext *ctx = nullptr;

    const char *deviceName = ":0";

    int ret = avformat_open_input(&ctx,deviceName,fmt,nullptr);

    if(ret !=0 ) {

        char errbuff[1024];

        av_strerror(ret,errbuff,sizeof(errbuff));

        qDebug() << "打开设备失败" << errbuff;

        return;
    }

    //第四步：采集数据
    QFile file = QFile("/Users/tonytaoliang/Downloads/out.pcm");

    if(!file.open(QIODeviceBase::WriteOnly)) {

        //关闭设备
        avformat_close_input(&ctx);

        qDebug() << "打开文件失败" << "out.pcm";
        return;
    }

    AVPacket packet;

    while (!isInterruptionRequested()) {

        ret = av_read_frame(ctx,&packet);

        if(ret == 0) {

            qDebug() << "我在写入文件" << ret;
            // 将数据写入文件
            file.write((const char*)packet.data,packet.size);

        }else if(ret == AVERROR(EAGAIN)) {
            //-35
            continue;

        }else {

            char errbuf[1024];
            av_strerror(ret,errbuf,sizeof(errbuf));
            qDebug() << "采集数据失败" << errbuf;
            break;
        }

    }

    //第五步：释放资源
    file.close();

    avformat_close_input(&ctx);
    qDebug() << this << "正常结束线程";

}

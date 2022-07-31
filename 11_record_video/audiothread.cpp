#include "audiothread.h"

#include <QDebug>
#include <QFile>
#include <QDateTime>

extern "C" {

#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>

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
    //设置参数
    AVDictionary *options = nullptr;
    av_dict_set(&options,"pixel_fromat","uyvy422",0);
    av_dict_set(&options,"video_size","1280x720",0);
    av_dict_set(&options,"framerate","30",0);

    //格式上下文（后面通过格式上下文操作设备）
    AVFormatContext *ctx = nullptr;

    const char *deviceName = "0";

    int ret = avformat_open_input(&ctx,deviceName,fmt,&options);

    //打开设备失败
    if(ret != 0 ){
        //传一个字符串数组
        char errorbuff[1024] = {0};
        av_strerror(ret,errorbuff,sizeof(errorbuff));
        qDebug() << "打开设备失败" << errorbuff;
        return;
    }

    //4.采集数据
    //创建一个文件，后面将采集到的数据写入文件
    QString filePath = "/Users/tonytaoliang/Downloads/out.yuv";

    QFile file = QFile(filePath);

    if(!file.open(QIODeviceBase::WriteOnly)) {

        //打开文件失败
        qDebug() << "文件打开失败" << filePath;
        // 关闭设备
        avformat_close_input(&ctx);
        return;
    }


    //计算每一帧的大小
    AVCodecParameters *param = ctx->streams[0]->codecpar;
    int imageSize = av_image_get_buffer_size((AVPixelFormat)param->format,param->width,param->height,1);

    AVPacket *packet = av_packet_alloc();

    //下面这种方式可知道错误码；当线程调用requestInterruption()时，isInterruptionRequested就会变为true
    while (!isInterruptionRequested()) {

        ret = av_read_frame(ctx, packet);

        if (ret == 0) { // 读取成功

            /*
            这里要使用imageSize，而不是pkt->size。
            pkt->size有可能比imageSize大（比如在Mac平台），
            使用pkt->size会导致写入一些多余数据到YUV文件中，
            进而导致YUV内容无法正常播放
            */
            // 将数据写入文件
            file.write((const char*)packet->data,imageSize);

        } else if (ret == AVERROR(EAGAIN)) { // 资源临时不可用#define EAGAIN   35 /* Resource temporarily unavailable */
            continue;
        } else { // 其他错误
            char errbuf[1024] = {0};
            av_strerror(ret, errbuf, sizeof (errbuf));
            qDebug() << "av_read_frame error" << errbuf << ret;
            break;

        }
        //释放packet的内部资源，如pkt->side_data
        av_packet_unref(packet);
    }
    //5.释放资源
    av_packet_free(&packet);

    file.close();

    avformat_close_input(&ctx);

    qDebug() << "线程正常结束";
}

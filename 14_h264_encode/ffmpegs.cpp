#include "ffmpegs.h"
#include <QDebug>
#include <QFile>
#include <QObject>

extern "C" {

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>

}

#define ERRORBUF(ret) \
    char errbuf[1024]; \
    av_strerror(ret,errbuf,sizeof(errbuf));


FFmpegs::FFmpegs()
{

}


// 检查编码器codec是否支持像素格式sample_fmt
static int check_sample_fmt(const AVCodec *codec, enum AVPixelFormat pix_fmt)
{
    const enum AVPixelFormat *p = codec->pix_fmts;

    while (*p != AV_PIX_FMT_NONE) {
        if (*p == pix_fmt)
            return 1;
        p++;
    }
    return 0;
}

//编码
static int encode(AVCodecContext *ctx,
                  AVFrame *frame,
                  AVPacket *pkt,
                  QFile &outFile) {

    int ret = avcodec_send_frame(ctx,frame);

    if (ret < 0) {
        ERRORBUF(ret);
        qDebug() << "avcodec_send_frame error" << errbuf;
        return ret;
    }

    while (true) {
        // 从编码器中获取编码后的数据
        ret = avcodec_receive_packet(ctx,pkt);

        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // packet中已经没有数据，需要重新发送数据到编码器（send frame）
            return 0;

        }else if (ret < 0) {
            //出现了其他错误
            ERRORBUF(ret);
            qDebug() << "avcodec_receive_packet error" << errbuf;
            return ret;
        }

        // 将编码后的数据写入文件
        outFile.write((char *)pkt->data,pkt->size);

        // 释放资源
        av_packet_unref(pkt);
    }

    return 0;
}

void FFmpegs::h264Encode(VideoEncodeSpec &in,
                         const char *outFilename){

    //返回值
    int ret = 0;

    //一帧的大小
    int imgSize = av_image_get_buffer_size(in.pixFormat,in.width,in.height,1);
    //输入文件
    QFile infile = QFile(in.fileName);

    //输出文件
    QFile outfile = QFile(outFilename);

    //编码器
    const AVCodec *codec = nullptr;

    //上下文
    AVCodecContext *ctx = nullptr;

    //输入缓冲区
    AVFrame *frame = nullptr;

    //输出缓冲区
    AVPacket *pkt = nullptr;

    //获取fdk_aac编码器
    codec = avcodec_find_encoder_by_name("libx264");

    if (!codec) {
        qDebug() << "encoder libx264 not found";
        return;
    }

    // 检查采样格式是否支持
    if(!check_sample_fmt(codec,in.pixFormat)) {

        qDebug() << "Encoder does not support pixel format"
                 << av_get_pix_fmt_name(in.pixFormat);
        return;

    }

    //创建上下文
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        qDebug() << "avcodec_alloc_context3 error";
        return;
    }

    //设置参数
    ctx->width = in.width;
    ctx->height = in.height;
    ctx->pix_fmt = in.pixFormat;
    //设置帧率（1秒钟显示的帧数是in.fps）
    ctx->time_base = {1,in.fps};

    //打开编码器
    ret = avcodec_open2(ctx,codec,nullptr);

    if(ret < 0){
        ERRORBUF(ret);
        qDebug() << "avcodec_open2 error" << errbuf;
        goto end;
    }

    //创建frame
    //this only allocates the AVFrame itself, not the data buffers. Those
    //must be allocated through other means, e.g. with av_frame_get_buffer() or manually.
    frame = av_frame_alloc();
    if (!frame) {
        qDebug() << "av_frame_alloc error";
        goto end;
    }

    frame->width = ctx->width;

    frame->height = ctx->height;

    frame->format = ctx->pix_fmt;

    frame->pts = 0;

    // 创建AVFrame内部的缓冲区方法一：
    ret = av_image_alloc(frame->data,frame->linesize,in.width,in.height,in.pixFormat,1);
    if (ret < 0) {
        ERRORBUF(ret);
        qDebug() << "av_image_alloc error" << errbuf;
        goto end;
    }

    // 创建AVFrame内部的缓冲区方法三：
    //    ret = av_frame_get_buffer(frame, 0);
    //    if (ret < 0) {
    //        ERRORBUF(ret);
    //        qDebug() << "av_frame_get_buffer error" << errbuf;
    //        goto end;
    //    }

    // 创建AVPacket
    pkt = av_packet_alloc();
    if (!pkt) {
        qDebug() << "av_packet_alloc error";
        goto end;
    }

    // 打开文件
    if (!infile.open(QFile::ReadOnly)) {
        qDebug() << "file open error" << in.fileName;
        goto end;
    }
    if (!outfile.open(QFile::WriteOnly)) {
        qDebug() << "file open error" << outFilename;
        goto end;
    }

    //开始编码 不传frame->linesize[0] 它只是Y平面一行的大小
    while ((ret = infile.read((char*)frame->data[0],imgSize)) > 0) {

        //编码
        if(encode(ctx,frame,pkt,outfile) < 0) {
            goto end;
        }

        frame->pts++;
    }

    //flush编码器
    encode(ctx,nullptr,pkt,outfile);

end:

    av_packet_free(&pkt);
    if (frame) {
        av_freep(frame->data[0]);
        av_frame_free(&frame);
    }

    avcodec_free_context(&ctx);
    infile.close();
    outfile.close();
}


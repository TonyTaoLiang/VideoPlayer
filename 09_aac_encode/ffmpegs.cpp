#include "ffmpegs.h"
#include <QDebug>
#include <QFile>
#include <QObject>

extern "C" {

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>

}

#define ERRORBUF(ret) \
    char errbuf[1024]; \
    av_strerror(ret,errbuf,sizeof(errbuf));


FFmpegs::FFmpegs()
{

}

//aac编码可以参考ffmpeg源码encode_audio.c
// 检查编码器codec是否支持采样格式sample_fmt
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
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

void FFmpegs::aacEncode(AudioEncodeSpec &in,
                        const char *outFilename){

    //返回值
    int ret = 0;

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
    codec = avcodec_find_encoder_by_name("libfdk_aac");
    //AVCodec *codec1 = avcodec_find_encoder(AV_CODEC_ID_AAC); //ffmpeg默认的aac编码器
    if (!codec) {
        qDebug() << "encoder libfdk_aac not found";
        return;
    }

    // 检查采样格式是否支持
    if(!check_sample_fmt(codec,in.sampleFormat)) {

        qDebug() << "Encoder does not support sample format"
                 << av_get_sample_fmt_name(in.sampleFormat);
        return;

    }

    //创建上下文
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        qDebug() << "avcodec_alloc_context3 error";
        return;
    }

    //设置参数
    ctx->bit_rate = 32000;//设置编码后的比特率。FF_PROFILE_AAC_HE_V2支持32000，不用太大
    ctx->sample_fmt = in.sampleFormat;
    ctx->channel_layout = in.ch_layout;
    ctx->sample_rate = in.sampleRate;
    //规格
    ctx->profile = FF_PROFILE_AAC_HE_V2;
    //如果想设置一些libfdk_aac特有的参数（比如vbr），可以通过options参数传递。设置可变比特率如下
//    AVDictionary *options = nullptr;
//    av_dict_set(&options, "vbr", "1", 0);

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

    // 样本帧数量（由frame_size决定）
    frame->nb_samples = ctx->frame_size;
    // 采样格式
    frame->format = ctx->sample_fmt;
    // 声道布局
    frame->channel_layout = ctx->channel_layout;
    // 创建AVFrame内部的缓冲区
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        ERRORBUF(ret);
        qDebug() << "av_frame_get_buffer error" << errbuf;
        goto end;
    }

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

    //开始编码
    while ((ret = infile.read((char*)frame->data[0],frame->linesize[0])) > 0) {


        //一开始是每次都读取并填满frame的缓冲区。但是最后一次可能不足以填满缓冲区，而我们还是将缓冲区全部拿去编码，导致最终的数据可能大一些
        if (ret < frame->linesize[0]) {

            //每个样本大小
            int bytes = av_get_bytes_per_sample((AVSampleFormat)frame->format);
            //声道数
            int channels = av_get_channel_layout_nb_channels(frame->channel_layout);
            //实际的样本帧数量（不是由上下文指定的大小ctx->frame_size）
            frame->nb_samples = ret / (bytes*channels);

        }


        //编码
        if(encode(ctx,frame,pkt,outfile) < 0) {
            goto end;
        }

    }

    //flush编码器
    encode(ctx,nullptr,pkt,outfile);

end:

    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&ctx);
    infile.close();
    outfile.close();
}


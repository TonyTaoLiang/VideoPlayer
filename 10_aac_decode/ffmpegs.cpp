#include "ffmpegs.h"
#include <QDebug>
#include <QFile>
#include <QObject>

extern "C" {

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>

}

// 输入缓冲区的大小
#define IN_DATA_SIZE 20480
// 需要再次读取输入文件数据的阈值
#define REFILL_THRESH 4096

#define ERRORBUF(ret) \
    char errbuf[1024]; \
    av_strerror(ret,errbuf,sizeof(errbuf));


FFmpegs::FFmpegs()
{

}

//解码
static int decode(AVCodecContext *ctx,
                  AVFrame *frame,
                  AVPacket *pkt,
                  QFile &outFile) {

    int ret = avcodec_send_packet(ctx,pkt);

    if (ret < 0) {
        ERRORBUF(ret);
        qDebug() << "avcodec_send_packet error" << errbuf;
        return ret;
    }

    while (true) {
        // 从解码器中获取解码后的数据
        ret = avcodec_receive_frame(ctx,frame);

        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // 已经没有数据，需要重新发送数据到解码器
            return 0;

        }else if (ret < 0) {
            //出现了其他错误
            ERRORBUF(ret);
            qDebug() << "avcodec_receive_frame error" << errbuf;
            return ret;
        }

        // 将解码后的数据写入文件
        outFile.write((char *)frame->data[0],frame->linesize[0]);
    }

    return 0;
}

void FFmpegs::aacDecode(AudioDecodeSpec &out,
                          const char *inFilename) {

    //返回值
    int ret = 0;

    // 每次从输入文件中读取的长度
    int inLen = 0;
    // 是否已经读取到了输入文件的尾部
    int inEnd = 0;

    //输入文件
    QFile infile = QFile(inFilename);

    //输出文件
    QFile outfile = QFile(out.fileName);

    // 用来存放读取的文件数据
    // 加上AV_INPUT_BUFFER_PADDING_SIZE是为了防止某些优化过的reader一次性读取过多导致越界
    char inDataArray[IN_DATA_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    char *inData = inDataArray;

    //编码器
    const AVCodec *codec = nullptr;

    //解析器上下文
    AVCodecParserContext *parserCtx = nullptr;

    //上下文
    AVCodecContext *ctx = nullptr;

    //输入缓冲区
    AVPacket *pkt = nullptr;

    //输出缓冲区
    AVFrame *frame = nullptr;

    //获取fdk_aac解码器
    codec = avcodec_find_decoder_by_name("libfdk_aac");
    //AVCodec *codec1 = avcodec_find_encoder(AV_CODEC_ID_AAC); //ffmpeg默认的aac编码器
    if (!codec) {
        qDebug() << "encoder libfdk_aac not found";
        return;
    }

    // 创建解析器上下文
    parserCtx = av_parser_init(codec->id);
    if (!parserCtx) {
        qDebug() << "av_parser_init error";
        return;
    }


    //创建上下文
    ctx = avcodec_alloc_context3(codec);

    if (!ctx) {
        qDebug() << "avcodec_alloc_context3 error";
        goto end;
    }

    //打开编码器
    ret = avcodec_open2(ctx,codec,nullptr);

    if(ret < 0){
        ERRORBUF(ret);
        qDebug() << "avcodec_open2 error" << errbuf;
        goto end;
    }

    //创建frame
    frame = av_frame_alloc();
    if (!frame) {
        qDebug() << "av_frame_alloc error";
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
        qDebug() << "file open error" << inFilename;
        goto end;
    }
    if (!outfile.open(QFile::WriteOnly)) {
        qDebug() << "file open error" << out.fileName;
        goto end;
    }

    //开始解码
    inLen = infile.read(inData,IN_DATA_SIZE);

    while (inLen > 0) {
        //return the number of bytes of the input bitstream used.
        //返回解析器实际上分割解析的那一小段
        ret = av_parser_parse2(parserCtx,ctx,
                               &pkt->data,&pkt->size,
                               (uint8_t *)inData,inLen,
                               AV_NOPTS_VALUE,AV_NOPTS_VALUE, 0);

        //缓冲区指针移动
        inData += ret;
        //减去已经解析过的数据大小,缓冲区未解析的数据慢慢缩短
        inLen -= ret;

        //解码
        //注意顺序：以下不要放在memmove后面，pkt指向indata同一块内存，修改这一块，indata是重置了，但pkt并没有，导致数据解码错误
        if(pkt->size > 0 && decode(ctx,frame,pkt,outfile) < 0) {
            goto end;
        }

        //判断阈值（因为缓冲区最后加了一个AV_INPUT_BUFFER_PADDING_SIZE=64，避免读到64这块区域，到了阈值就提前重新继续读取文件）
        if (inLen <= REFILL_THRESH && !inEnd) {

            //这一次缓冲区剩下的数据前移，后面继续拼接新读取的文件
            memmove(inDataArray,inData,inLen);
            //重置指针，再次移动到缓冲区最前端
            inData = inDataArray;

            //继续读取文件
            int len = infile.read(inData + inLen,IN_DATA_SIZE - inLen);

            if (len > 0) {

                //确实又读到了数据
                inLen += len;


            }else {

                //文件已经读完了，设置flag，不用再继续读了
                inEnd = 1;
            }

        }

    }

    //flush解码器
    decode(ctx,frame,nullptr,outfile);

    // 设置输出参数(我们解码过程中并未特地设置这些，aac中应该有这些数据)
    //这里只是做个验证，调用解码函数，打印下out的以下三要素，看看是不是设置进去了
    out.sampleRate = ctx->sample_rate;
    out.sampleFormat = ctx->sample_fmt;
    out.ch_layout = ctx->channel_layout;

end:

    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&ctx);
    av_parser_close(parserCtx);
    infile.close();
    outfile.close();
}


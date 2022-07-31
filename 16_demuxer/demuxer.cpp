#include "demuxer.h"
#include <QDebug>

extern "C" {
#include <libavutil/imgutils.h>
}

#define ERROR_BUF \
    char errbuf[1024]; \
    av_strerror(ret, errbuf, sizeof (errbuf));

#define END(func) \
    if (ret < 0) { \
    ERROR_BUF; \
    qDebug() << #func << "error" << errbuf; \
    goto end; \
    }

#define RET(func) \
    if (ret < 0) { \
    ERROR_BUF; \
    qDebug() << #func << "error" << errbuf; \
    return ret; \
    }

demuxer::demuxer()
{

}

void demuxer::demux(const char *inFilename,
                    AudioDecodeSpec &aOut,
                    VideoDecodeSpec &vOut) {

    _aOut = &aOut;
    _vOut = &vOut;

    AVPacket *pkt = nullptr;

    // 返回结果
    int ret = 0;

    // 创建解封装上下文、打开文件
    ret = avformat_open_input(&_fmtCtx,inFilename,nullptr,nullptr);
    END(avformat_open_input);

    // 检索流信息
    ret = avformat_find_stream_info(_fmtCtx,nullptr);
    END(avformat_find_stream_info);

    // 打印流信息到控制台
    av_dump_format(_fmtCtx, 0, inFilename, 0);
    fflush(stderr);

    // 初始化音频信息
    ret = initAudioInfo();
    if (ret < 0) {
        goto end;
    }

    // 初始化视频信息
    ret = initVideoInfo();
    if (ret < 0) {
        goto end;
    }

    // 初始化frame
    _frame = av_frame_alloc();
    if (!_frame) {
        qDebug() << "av_frame_alloc error";
        goto end;
    }

    // 初始化pkt
    pkt = av_packet_alloc();
    pkt->data = nullptr;
    pkt->size = 0;


    // 从输入文件中读取数据
    while (av_read_frame(_fmtCtx,pkt) == 0) {

        if (pkt->stream_index == AVMEDIA_TYPE_VIDEO) {

            ret = decode(_vDecodeCtx,pkt,&demuxer::writeVideoFrame);

        }else if (pkt->stream_index == AVMEDIA_TYPE_AUDIO){

            ret = decode(_aDecodeCtx,pkt,&demuxer::writeAudioFrame);
        }

        // 释放pkt内部指针指向的一些额外内存
        av_packet_unref(pkt);

        if (ret < 0) {
            goto end;
        }
    }

    // 刷新缓冲区
    decode(_aDecodeCtx, nullptr,&demuxer::writeAudioFrame);
    decode(_vDecodeCtx, nullptr,&demuxer::writeVideoFrame);
end:
    avformat_close_input(&_fmtCtx);
    _aOutFile.close();
    _vOutFile.close();
    avcodec_free_context(&_aDecodeCtx);
    avcodec_free_context(&_vDecodeCtx);
    av_frame_free(&_frame);
    av_packet_free(&pkt);
}

int demuxer::initVideoInfo(){

    int ret = initDecoder(&_vDecodeCtx,&_vStreamIdx,AVMEDIA_TYPE_VIDEO);
    RET(initDecoder);

    // 打开文件
    _vOutFile.setFileName(_vOut->filename);
    if (!_vOutFile.open(QFile::WriteOnly)) {
        qDebug() << "file open error" << _vOut->filename;
        return -1;
    }

    // 保存视频参数
    _vOut->width = _vDecodeCtx->width;
    _vOut->height = _vDecodeCtx->height;
    _vOut->pixFmt = _vDecodeCtx->pix_fmt;
    // 帧率
    AVRational framerate = av_guess_frame_rate(
                _fmtCtx,
                _fmtCtx->streams[_vStreamIdx],
                nullptr);
    _vOut->fps = framerate.num / framerate.den;

    //创建一帧缓冲区，方便writeVideoFrame
    ret = av_image_alloc(_imgBuf,_imgLinesizes,_vOut->width,_vOut->height,_vOut->pixFmt,1);
    RET(av_image_alloc);
    //将返回的一帧大小赋值
    _imgSize = ret;

    return 0;
}

int demuxer::initAudioInfo(){

    // 初始化解码器
    int ret = initDecoder(&_aDecodeCtx, &_aStreamIdx, AVMEDIA_TYPE_AUDIO);
    RET(initDecoder);

    // 打开文件
    _aOutFile.setFileName(_aOut->filename);
    if (!_aOutFile.open(QFile::WriteOnly)) {
        qDebug() << "file open error" << _aOut->filename;
        return -1;
    }

    // 保存音频参数
    _aOut->sampleRate = _aDecodeCtx->sample_rate;
    _aOut->sampleFmt = _aDecodeCtx->sample_fmt;
    _aOut->chLayout = _aDecodeCtx->channel_layout;

    // 每一个音频样本的大小（单声道）
    _sampleSize = av_get_bytes_per_sample(_aOut->sampleFmt);
    // 每个音频样本帧（包含所有声道）的大小
    _sampleFrameSize = _sampleSize * _aDecodeCtx->channels;

    return 0;
}

// 初始化解码器
int demuxer::initDecoder(AVCodecContext **decodeCtx,
                         int *streamIdx,
                         AVMediaType type) {

    // 根据type寻找最合适的流信息
    // 返回值是流索引
    int ret = av_find_best_stream(_fmtCtx, type, -1, -1, nullptr, 0);
    RET(av_find_best_stream);

    //传出流索引
    *streamIdx = ret;

    // 检验流
    AVStream *stream = _fmtCtx->streams[*streamIdx];
    if(!stream) {
        qDebug() << "stream is empty";
        return -1;
    }

    // 为当前流找到合适的解码器
    const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
        qDebug() << "decoder not found" << stream->codecpar->codec_id;
        return -1;
    }

    // 初始化解码上下文
    *decodeCtx = avcodec_alloc_context3(decoder);
    if (!decodeCtx) {
        qDebug() << "avcodec_alloc_context3 error";
        return -1;
    }

    // 从流中拷贝参数到解码上下文中
    ret = avcodec_parameters_to_context(*decodeCtx, stream->codecpar);
    RET(avcodec_parameters_to_context);

    // 打开解码器
    ret = avcodec_open2(*decodeCtx, decoder, nullptr);
    RET(avcodec_open2);

    return 0;
}

// 解码
int demuxer::decode(AVCodecContext *decodeCtx,
                    AVPacket *pkt, void (demuxer::*func)()) {
    // 发送压缩数据到解码器
    int ret = avcodec_send_packet(decodeCtx, pkt);
    RET(avcodec_send_packet);

    while (true) {
        // 获取解码后的数据
        ret = avcodec_receive_frame(decodeCtx, _frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }
        RET(avcodec_receive_frame);

        // 执行写入文件的代码 直接调用函数，不再向下面再次判断type
        (this->*func)();

        // 将frame的数据写入文件
        //        if (decodeCtx->codec->type == AVMEDIA_TYPE_VIDEO) {
        //            writeVideoFrame();
        //        } else {
        //            writeAudioFrame();
        //        }

    }
}

void demuxer::writeVideoFrame() {
    //    // 写入Y平面
    //    _vOutFile.write((char *) _frame->data[0],
    //            _frame->linesize[0] * _vOut->height);
    //    // 写入U平面
    //    _vOutFile.write((char *) _frame->data[1],
    //            _frame->linesize[1] * _vOut->height >> 1);
    //    // 写入V平面
    //    _vOutFile.write((char *) _frame->data[2],
    //            _frame->linesize[2] * _vOut->height >> 1);

    //选择一个通用的方法来写入，不一定是yuv420p
    av_image_copy(_imgBuf,_imgLinesizes,(const uint8_t **)_frame->data,_frame->linesize,_vOut->pixFmt,_vOut->width,_vOut->height);
    _vOutFile.write((char*)_imgBuf[0],_imgSize);
}

void demuxer::writeAudioFrame() {

    // libfdk_aac解码器，解码出来的PCM格式：s16
        // aac解码器，解码出来的PCM格式：ftlp
    //注意区分planer和非planer
    if (av_sample_fmt_is_planar(_aOut->sampleFmt)){//planer

        for(int si = 0 ; si < _frame->nb_samples; si++) {// 外层循环：每一个声道的样本数

            for (int ci = 0 ; ci < _frame->channels; ci++) {//声道数

                //指针单位是uint_t8*是一个字节，但是这里跳转的是一个声道的大小，选取一个个声道
                char * begin = (char *)(_frame->data[ci] + _sampleSize * si);

                _aOutFile.write(begin,_sampleSize);

            }

        }


    }else {

        _aOutFile.write((char *)_frame->data[0],_frame->nb_samples * _sampleFrameSize);
    }

}

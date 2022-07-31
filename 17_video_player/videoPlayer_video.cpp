#include "videoplayer.h"
#include <QDebug>
#include <thread>

extern "C" {

#include <libavutil/imgutils.h>

}

int videoPlayer::initVideoInfo(){

    //初始化解码器
    int ret = initDecoder(&_vDecodeCtx,&_vStream,AVMEDIA_TYPE_VIDEO);
    RET(initDecoder);

    //初始化像素格式转换
    ret = initSws();
    RET(initSws);

    return 0;
}

int videoPlayer::initSws() {

    int inW = _vDecodeCtx->width;
    int inH = _vDecodeCtx->height;

    // 输出frame的参数
    _vSwsOutSpec.width = inW >> 4 << 4;
    _vSwsOutSpec.height = inH >> 4 << 4;
    _vSwsOutSpec.pixFmt = AV_PIX_FMT_RGB24;
    _vSwsOutSpec.size = av_image_get_buffer_size(
                _vSwsOutSpec.pixFmt,
                _vSwsOutSpec.width,
                _vSwsOutSpec.height, 1);

    // 初始化像素格式转换的上下文
    _vSwsCtx = sws_getContext(inW,
                              inH,
                              _vDecodeCtx->pix_fmt,
                              _vSwsOutSpec.width,
                              _vSwsOutSpec.height,
                              _vSwsOutSpec.pixFmt,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!_vSwsCtx) {
        qDebug() << "sws_getContext error";
        return -1;
    }

    //初始化输入缓冲区_aSwrInFrame
    _vSwsInFrame = av_frame_alloc();
    if(!_vSwsInFrame) {
        qDebug() << "av_frame_alloc error";
        return -1;
    }

    //初始化输出缓冲区
    _vSwsOutFrame = av_frame_alloc();
    if (!_vSwsOutFrame) {
        qDebug() << "av_frame_alloc error";
        return -1;
    }

    // _vSwsOutFrame的data[0]指向的内存空间
    int ret = av_image_alloc(_vSwsOutFrame->data,
                             _vSwsOutFrame->linesize,
                             _vSwsOutSpec.width,
                             _vSwsOutSpec.height,
                             _vSwsOutSpec.pixFmt,
                             1);
    RET(av_image_alloc);

    return 0;
}

void videoPlayer::addVideoPkt(AVPacket &pkt) {

    _vMutex.lock();
    _vPktList.push_back(pkt);
    _vMutex.signal();
    _vMutex.unlock();

}

void videoPlayer::decodeVideo() {

    while (true) {

        // 如果是暂停，并且没有Seek操作。为了暂停时也能seek
        if (_state == Paused && _vSeekTime == -1) {
            continue;
        }

        if (_state == Stopped) {
            _vCanFree = true;
            break;
        }

        _vMutex.lock();

        if (_vPktList.empty() || _state == Stopped) {
            _vMutex.unlock();
            continue;
        }

        AVPacket pkt = _vPktList.front();

        _vPktList.pop_front();

        _vMutex.unlock();

        // 视频时钟 这里是dts 音频是pts
        // DTS:Decompression timestamp PTS:Presentation timestamp
        //PTS主要用于度量解码后的视频帧什么时候被显示出来。
        //DTS主要是标识读入内存中的比特流在什么时候开始送入解码器中进行解码
        //pts MUST be larger or equal to dts as presentation cannot happen before decompression(pts要大于dts因为显示不能比解压缩早)
        if (pkt.dts != AV_NOPTS_VALUE) {
            _vTime = av_q2d(_vStream->time_base) * pkt.dts;
        }

        int ret = avcodec_send_packet(_vDecodeCtx,&pkt);

        //用完pkt就把pkt里面的data清空
        av_packet_unref(&pkt);

        CONTINUE(avcodec_send_packet);

        while (true) {
            // 从解码器中获取解码后的数据（视频解码不像音频可能多个样本，这里就是一帧）
            //_vSwsInFrame不需要像_vSwsOutFrame创建里面的data，因为avcodec_receive_frame这个方法内部自己创建了
            ret = avcodec_receive_frame(_vDecodeCtx,_vSwsInFrame);

            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                // 已经没有数据，需要重新发送数据到解码器
                break;

            }else BREAK(avcodec_receive_frame);

            // 如果是视频，不能在这个位置(要在avcodec_send_packet送入packet后，B，P帧需要参考之前的帧)判断（不能提前释放pkt，不然会导致B帧、P帧解码失败，画面撕裂）
            // 和音频不一样，一定要在解码成功后，再进行下面的判断
            // 发现视频的时间是早于seekTime的，直接丢弃
            if (_vSeekTime >= 0) {
                if (_vTime < _vSeekTime) {
                    continue;
                } else {
                    _vSeekTime = -1;
                }
            }

            if (_hasAudio) { // 有音频
                // 如果视频包过早被解码出来，那就需要等待对应的音频时钟到达
                while (_vTime > _aTime && _state == Playing) {
                    //也可以一直在这空转，只要不把这一帧拿出去渲染emit frameDecoded(this, _vSwsOutFrame->data[0], _vSwsOutSpec);
                    //                    SDL_Delay(5);
                }
            } else {
                // TODO 没有音频的情况

            }


            //像素格式转换
            sws_scale(_vSwsCtx,
                      _vSwsInFrame->data, _vSwsInFrame->linesize,
                      0, _vDecodeCtx->height,
                      _vSwsOutFrame->data, _vSwsOutFrame->linesize);
            //把像素格式转换后的图片数据，拷贝一份出来.Qimage这个主线成用的data在子线程也在用,可能有问题
            //av_malloc,然后所以videowidget释放image时就需要释放bits 。这也是晚上那次为什么说free了未alloc的东西
            //_vSwsOutSpec.size：一帧的大小
            uint8_t *data = (uint8_t *)av_malloc(_vSwsOutSpec.size);
            memcpy(data,_vSwsOutFrame->data[0],_vSwsOutSpec.size);
            emit frameDecoded(this, data, _vSwsOutSpec);
        }
    }

}

void videoPlayer::clearVideoPktList() {

    _vMutex.lock();
    for (AVPacket &pkt : _vPktList) {
        av_packet_unref(&pkt);
    }
    _vPktList.clear();
    _vMutex.unlock();
}

void videoPlayer::freeVideo() {


    clearVideoPktList();
    avcodec_free_context(&_vDecodeCtx);
    av_frame_free(&_vSwsInFrame);
    if (_vSwsOutFrame) {
        av_freep(&_vSwsOutFrame->data[0]);
        av_frame_free(&_vSwsOutFrame);
    }
    sws_freeContext(_vSwsCtx);
    _vSwsCtx = nullptr;
    _vStream = nullptr;
    _vTime = 0;
    _vCanFree = false;
    _vSeekTime = -1;
}
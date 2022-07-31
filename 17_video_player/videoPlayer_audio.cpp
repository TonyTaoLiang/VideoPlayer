#include <SDL2/SDL.h>
#include "videoplayer.h"
#include <QDebug>


int videoPlayer::initAudioInfo(){

    // 初始化解码器
    int ret = initDecoder(&_aDecodeCtx, &_aStream, AVMEDIA_TYPE_AUDIO);
    RET(initDecoder);

    //初始化重采样
    ret = initSwr();
    RET(initSwr);

    //初始化SDL
    ret = initSDL();
    RET(initSDL);
    return 0;
}

int videoPlayer::initSwr() {

    //输入参数
    _aSwrInSpec.sampleFmt = _aDecodeCtx->sample_fmt;
    _aSwrInSpec.sampleRate = _aDecodeCtx->sample_rate;
    _aSwrInSpec.chLayout = _aDecodeCtx->channel_layout;
    _aSwrInSpec.chs = _aDecodeCtx->channels;

    //输出参数
    _aSwrOutSpec.sampleFmt = AV_SAMPLE_FMT_S16;
    _aSwrOutSpec.sampleRate = 44100;
    _aSwrOutSpec.chLayout = AV_CH_LAYOUT_STEREO;
    _aSwrOutSpec.chs = av_get_channel_layout_nb_channels(_aSwrOutSpec.chLayout);
    _aSwrOutSpec.bytesPerSampleFrame = _aSwrOutSpec.chs
            * av_get_bytes_per_sample(_aSwrOutSpec.sampleFmt);


    // 创建重采样上下文
    _aSwrCtx = swr_alloc_set_opts(nullptr,
                                  // 输出参数
                                  _aSwrOutSpec.chLayout,
                                  _aSwrOutSpec.sampleFmt,
                                  _aSwrOutSpec.sampleRate,
                                  // 输入参数
                                  _aSwrInSpec.chLayout,
                                  _aSwrInSpec.sampleFmt,
                                  _aSwrInSpec.sampleRate,
                                  0, nullptr);
    if (!_aSwrCtx) {
        qDebug() << "swr_alloc_set_opts error";
        return -1;
    }

    // 初始化重采样上下文
    int ret = swr_init(_aSwrCtx);
    RET(swr_init);


    //初始化输入缓冲区_aSwrInFrame
    _aSwrInFrame = av_frame_alloc();
    if(!_aSwrInFrame) {
        qDebug() << "av_frame_alloc error";
        return -1;
    }

    //初始化输出缓冲区
    _aSwrOutFrame = av_frame_alloc();
    if (!_aSwrOutFrame) {
        qDebug() << "av_frame_alloc error";
        return -1;
    }

    // 输出缓冲区还需要手动创建AVFrame内部的data缓冲区,初始化data指针
    //    int ret = av_frame_get_buffer(_aSwrOutFrame, 0);
    //    RET(av_frame_get_buffer);
    //重采样缓冲区大小4096个样本
    ret = av_samples_alloc(_aSwrOutFrame->data,_aSwrInFrame->linesize,_aSwrOutSpec.chs,4096,_aSwrOutSpec.sampleFmt,1);
    RET(av_samples_alloc);

    return 0;

}

int videoPlayer::initSDL() {
    // 音频参数
    SDL_AudioSpec spec;
    // 采样率
    spec.freq = 44100;
    // 采样格式（s16le）
    spec.format = AUDIO_S16LSB;
    // 声道数
    spec.channels = 2;
    // 音频缓冲区的样本数量（这个值必须是2的幂）
    spec.samples = 512;
    // 回调
    spec.callback = sdlAudioCallbackFunc;
    // 传递给回调的参数
    spec.userdata = this;

    // 打开音频设备
    if (SDL_OpenAudio(&spec, nullptr)) {
        qDebug() << "SDL_OpenAudio error" << SDL_GetError();
        return -1;
    }

    return 0;
}

void videoPlayer::sdlAudioCallbackFunc(void *userdata, Uint8 *stream, int len) {
    //这个是SDL自己创建的子线程
    videoPlayer *player = (videoPlayer *) userdata;
    player->sdlAudioCallback(stream, len);
}

void videoPlayer::sdlAudioCallback(Uint8 *stream, int len) {

    // 清零（静音）
    SDL_memset(stream, 0, len);

    /*************这地方的代码重要******************/
    // len：SDL音频缓冲区剩余的大小（还未填充的大小）
    while (len > 0) {

        if (_state == Paused) break; //暂停
        if (_state == Stopped) {
            _aCanFree = true;
            break;
        }

        // 说明当前PCM的数据已经全部拷贝到SDL的音频缓冲区了
        // 需要解码下一个pkt，获取新的PCM数据
        if (_aSwrOutIdx >= _aSwrOutSize) {

            //全新的pcm的大小
            _aSwrOutSize = decodeAudio();
            //索引清0
            _aSwrOutIdx = 0;

            // 没有解码出PCM数据，那就静音处理
            if (_aSwrOutSize <= 0) {

                //假定PCM的大小1024
                _aSwrOutSize = 1024;
                //手动清零，1024个字节够了,然后设置进SDL缓冲区
                memset(_aSwrOutFrame->data[0],0,_aSwrOutSize);

            }

        }


        // 本次需要填充到stream中的PCM数据大小
        int fillLen = _aSwrOutSize - _aSwrOutIdx;
        // 比较SDL剩余的空间，和PCM要填充的大小，选小的
        fillLen = std::min(fillLen,len);

        // 获取当前音量
        int volumn = _mute ? 0 : ((_volumn * 1.0 / Max) * SDL_MIX_MAXVOLUME);

        //填充SDL缓冲区
        SDL_MixAudio(stream,
                     _aSwrOutFrame->data[0] + _aSwrOutIdx,
                fillLen, volumn);

        len -= fillLen;
        stream += fillLen;
        _aSwrOutIdx += fillLen;
    }

}

// & 引用：披着对象外衣的指针 调用addAudioPkt传入pkt时就不会拷贝构造(又创建了一次对象)，只是传地址。只有里面_aPktList.push_back拷贝构造一次而已。
// 如果参数是AVPacket pkt，那么调用相当于AVPacket pkt = packet（外面传进来的）这就又是一次拷贝构造 没有必要
// 引用的其他细节可以参考c++课程
void videoPlayer:: addAudioPkt(AVPacket &pkt) {

    _aMutex.lock();
    _aPktList.push_back(pkt);
    _aMutex.signal();
    _aMutex.unlock();
}

void videoPlayer::clearAudioPktList() {

    _aMutex.lock();
    for (AVPacket &pkt : _aPktList) {
        av_packet_unref(&pkt);
    }
    _aPktList.clear();
    _aMutex.unlock();
}

int videoPlayer::decodeAudio() {

    _aMutex.lock();

    if (_aPktList.empty() || _state == Stopped) {
        _aMutex.unlock();
        return 0;
    }

    // 如果加了& 引用就不会拷贝构造，pop_front销毁_aPktList内部的AVPacket对象，后面avcodec_send_packet(_aDecodeCtx,&pkt);还拿着用直接崩溃
    // 如果非要加& 就pop_front()方法挪到后面
    //AVPacket &pkt = _aPktList.front();
    //取出头部数据包：拷贝构造 就会创建一个新的AVPacket对象，即使pop_front销毁_aPktList内部的AVPacket对象也无所谓
    AVPacket pkt = _aPktList.front();

    _aPktList.pop_front();

    _aMutex.unlock();

    // 保存音频时钟(同步音视频)
    //pkt.pts是当前包的时间，单位是_aStream->time_base，将其换算成秒
    if (pkt.pts != AV_NOPTS_VALUE) {
        _aTime = av_q2d(_aStream->time_base) * pkt.pts;
        // 通知外界：播放时间点发生了改变
        emit timeChanged(this);
    }

    // 丢掉seek位置之前的包，解决seek时快速几帧闪过的问题，可能之前的帧已经送入packet了
    // 如果是视频，不能在这个位置(要在avcodec_send_packet送入packet后，B，P帧需要参考之前的帧)判断（不能提前释放pkt，不然会导致B帧、P帧解码失败，画面撕裂）
    // 发现音频的时间是早于seekTime的，直接丢弃
    if (_aSeekTime >= 0) {
        if (_aTime < _aSeekTime) {
            // 释放pkt
            av_packet_unref(&pkt);
            return 0;
        } else {
            _aSeekTime = -1;
        }
    }


    int ret = avcodec_send_packet(_aDecodeCtx,&pkt);

    //用完pkt就把pkt里面的data清空
    av_packet_unref(&pkt);

    RET(avcodec_send_packet);

    // 从解码器中获取解码后的数据
    ret = avcodec_receive_frame(_aDecodeCtx,_aSwrInFrame);

    if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        // 已经没有数据，需要重新发送数据到解码器
        return 0;

    }else RET(avcodec_receive_frame);


    //重采样歌曲的时长是不变的，采样率不同则导致输入/输出缓冲区的样本数量应该不同
    //inSampleRate / outSampleRate = inSamples /  outSamples 向上取整
    int out_nb_samples = av_rescale_rnd(_aSwrInFrame->nb_samples,_aSwrOutSpec.sampleRate,_aSwrInSpec.sampleRate,AV_ROUND_UP);

    //重采样 由于解码出来的PCM。跟SDL要求的PCM格式可能不一致
    //返回输出的样本数量 number of samples output per channel
    ret = swr_convert(_aSwrCtx,
                      _aSwrOutFrame->data,
                      out_nb_samples,
                      (const uint8_t **)_aSwrInFrame->data,
                      _aSwrInFrame->nb_samples);

    RET(swr_convert);

    return ret *_aSwrOutSpec.bytesPerSampleFrame;

}

void videoPlayer::freeAudio() {

    _aTime = 0;
    _aSwrOutIdx = 0;
    _aSwrOutSize = 0;
    _aStream = nullptr;
    _aCanFree = false;
    _aSeekTime = -1;
    clearAudioPktList();
    avcodec_free_context(&_aDecodeCtx);
    swr_free(&_aSwrCtx);
    av_frame_free(&_aSwrInFrame);
    if (_aSwrOutFrame) {
        av_freep(&_aSwrOutFrame->data[0]);
        av_frame_free(&_aSwrOutFrame);
    }

    // 停止播放
    SDL_PauseAudio(1);
    SDL_CloseAudio();
}

#include "videoplayer.h"
#include <QDebug>
#include <thread>

#define AUDIO_MAX_PKT_SIZE 1000
#define VIDEO_MAX_PKT_SIZE 500

videoPlayer::videoPlayer(QObject *parent)
    : QObject{parent}
{  

    if(SDL_Init(SDL_INIT_AUDIO)){
        qDebug() << "初始化SDLAudio失败" << SDL_GetError();
        emit playFailed(this);
        return;
    }

}

videoPlayer::~videoPlayer(){

    // 不再对外发送消息
    disconnect();
    stop();
    SDL_Quit();

}

void videoPlayer::setFilename(QString &filename) {

    const char *name = filename.toStdString().c_str();
    memcpy(_filename, name, strlen(name) + 1);
}

#pragma mark - 公共方法
void videoPlayer::play() {

    if (_state == Playing) return;
    // 状态可能是：暂停、停止、正常完毕

    if (_state == Stopped) {

        //读取文件
        std::thread([this](){

            readFile();

        }).detach();

    } else {
        // 改变状态
        setState(Playing);
    }
}

void videoPlayer::pause() {
    if (_state != Playing) return;
    // 状态可能是：正在播放

    // 改变状态
    setState(Paused);
}

void videoPlayer::stop() {
    if (_state == Stopped) return;
    // 状态可能是：正在播放、暂停、正常完毕

    // 改变状态
    _state = Stopped;

    // 释放资源
    free();

    // 通知外界
    emit stateChanged(this);

}

bool videoPlayer::isPlaying() {
    return _state == Playing;
}

videoPlayer::State videoPlayer::getState() {
    return _state;
}

#pragma mark - 私有方法
void videoPlayer::setState(State state) {
    if (state == _state) return;

    _state = state;

    emit stateChanged(this);
}

void videoPlayer::readFile() {

    // 返回结果
    int ret = 0;

    // 创建解封装上下文、打开文件
    ret = avformat_open_input(&_fmtCtx,_filename,nullptr,nullptr);
    END(avformat_open_input);

    // 检索流信息
    ret = avformat_find_stream_info(_fmtCtx,nullptr);
    END(avformat_find_stream_info);

    // 打印流信息到控制台
    av_dump_format(_fmtCtx, 0, _filename, 0);
    fflush(stderr);

    // 初始化音频信息
    _hasAudio = initAudioInfo() >= 0;

    // 初始化视频信息
    _hasVideo = initVideoInfo() >= 0;

    if (!_hasAudio && !_hasVideo) {
        fataError();
        return;
    }

    // 到此为止，初始化完毕
    emit initFinished(this);

    // 改变状态
    setState(Playing);

    // 音频解码子线程：开始工作
    SDL_PauseAudio(0);

    //视频解码子线程：开始工作
    std::thread([this](){

        decodeVideo();

    }).detach();


    // 从输入文件中读取数据
    while(_state != Stopped) {

        // 处理seek操作
        if(_seekTime >=0) {

            //流索引
            int streamIdx;
            if (_hasAudio) { // 优先使用音频流索引
                streamIdx = _aStream->index;
            } else {
                streamIdx = _vStream->index;
            }

            // 现实时间 -> 时间戳
            AVRational timeBase = _fmtCtx->streams[streamIdx]->time_base;
            int64_t ts = _seekTime / av_q2d(timeBase);
            ret = av_seek_frame(_fmtCtx, streamIdx, ts, AVSEEK_FLAG_BACKWARD);

            if (ret < 0) { // seek失败
                qDebug() << "seek失败" << _seekTime << ts << streamIdx;
                _seekTime = -1;
            } else {
                qDebug() << "seek成功" << _seekTime << ts << streamIdx;
                // 先清空之前读取的数据包，再设置seekTime就不会出现往前seek卡住的问题
                clearAudioPktList();
                clearVideoPktList();

                _vSeekTime = _seekTime;
                _aSeekTime = _seekTime;
                _seekTime = -1;
                // 恢复时钟
                _aTime = 0;
                _vTime = 0;
                // 清空之前读取的数据包
//                clearAudioPktList();
//                clearVideoPktList();
            }
        }


        //限制一下，不要无限读取到内存
        if (_aPktList.size() >= AUDIO_MAX_PKT_SIZE || _vPktList.size() >= VIDEO_MAX_PKT_SIZE) {
            continue;
        }

        AVPacket pkt;
        ret =  av_read_frame(_fmtCtx,&pkt);

        if (ret == 0) {

            if (pkt.stream_index == AVMEDIA_TYPE_VIDEO) {

                addVideoPkt(pkt);

            }else if (pkt.stream_index == AVMEDIA_TYPE_AUDIO){

                addAudioPkt(pkt);
            } else { // 如果不是音频、视频流，直接释放
                av_packet_unref(&pkt);
            }

        }else if (ret == AVERROR_EOF) {//读取到了文件尾部
            //break;不要break跳出循环，等待用户seek操作
            if (_vPktList.size() == 0 && _aPktList.size() == 0) {
                // 说明文件正常播放完毕 包里没东西了
                _fmtCtxCanFree = true;
                break;
            }
        }else {
            ERROR_BUF;
            qDebug() << "av_read_frame error" << errbuf;
            continue;
        }
    }

    if (_fmtCtxCanFree) { // 文件正常播放完毕，就停止
        stop();
    } else {
        // 标记一下：_fmtCtx可以释放了
        _fmtCtxCanFree = true;
    }

    // 刷新缓冲区
    //        decode(_aDecodeCtx, nullptr,&videoPlayer::writeAudioFrame);
    //        decode(_vDecodeCtx, nullptr,&videoPlayer::writeVideoFrame);

}

// 初始化解码器
int videoPlayer::initDecoder(AVCodecContext **decodeCtx,
                             AVStream **stream,
                             AVMediaType type) {

    // 根据type寻找最合适的流信息
    // 返回值是流索引
    int ret = av_find_best_stream(_fmtCtx, type, -1, -1, nullptr, 0);
    RET(av_find_best_stream);

    //传出流
    int streamIdx = ret;
    *stream = _fmtCtx->streams[streamIdx];
    // 检验流
    if(!*stream) {
        qDebug() << "stream is empty";
        return -1;
    }

    // 为当前流找到合适的解码器
    const AVCodec *decoder = avcodec_find_decoder((*stream)->codecpar->codec_id);
    if (!decoder) {
        qDebug() << "decoder not found" << (*stream)->codecpar->codec_id;
        return -1;
    }

    // 初始化解码上下文
    *decodeCtx = avcodec_alloc_context3(decoder);
    if (!decodeCtx) {
        qDebug() << "avcodec_alloc_context3 error";
        return -1;
    }

    // 从流中拷贝参数到解码上下文中
    ret = avcodec_parameters_to_context(*decodeCtx, (*stream)->codecpar);
    RET(avcodec_parameters_to_context);

    // 打开解码器
    ret = avcodec_open2(*decodeCtx, decoder, nullptr);
    RET(avcodec_open2);

    return 0;
}

int videoPlayer::getDuration() {
    //round 四舍五入
    return _fmtCtx ? round(_fmtCtx->duration * av_q2d(AV_TIME_BASE_Q)) : 0;
    // return _fmtCtx ? round(_fmtCtx->duration / 1000000.0) : 0;
}

int videoPlayer::getTime() {
    return round(_aTime);
}

void videoPlayer::setTime(int seekTime){
    _seekTime = seekTime;
}

void videoPlayer::setVolumn(int volumn) {
    _volumn = volumn;
}

int videoPlayer::getVolumn() {
    return _volumn;
}

void videoPlayer::setMute(bool mute) {
    _mute = mute;
}

bool videoPlayer::isMute() {
    return _mute;
}

void videoPlayer::free() {
    while (_hasAudio && !_aCanFree);
    while (_hasVideo && !_vCanFree);
    while (!_fmtCtxCanFree);
    avformat_close_input(&_fmtCtx);
    _fmtCtxCanFree = false;
    _seekTime = -1;
    freeAudio();
    freeVideo();
}

void videoPlayer::fataError() {

    // 配合stop能够调用成功
    _state = Playing;
    stop();
    emit playFailed(this);
}

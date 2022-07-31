#include "yuvplayer.h"
#include <qdebug.h>
#include <QPainter>
#include "ffmpegs.h"

extern "C" {
#include <libavutil/imgutils.h>
}

yuvplayer::yuvplayer(QWidget *parent)
    : QWidget{parent}
{
    // 设置背景色
    setAttribute(Qt::WA_StyledBackground);
    //    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: black");
}

yuvplayer::~yuvplayer(){

    closeFile();
    freeCurrentImage();
    stopTimer();

}

void yuvplayer::play() {


    if (_state == Playing) return;

    //定时器的间隔时间就是刷新每一帧的时间_interval = 1000 / _yuv.fps;
    _timerId = startTimer(_interval);

    setState(Playing);

}

void yuvplayer::pause() {
    if (_state != Playing) return;

    // 状态可能是：正在播放

    // 停止定时器
    stopTimer();

    // 改变状态
    setState(Paused);
}

void yuvplayer::stop() {
    if (_state == Stopped) return;

    // 状态可能是：正在播放、暂停、正常完毕

    // 停止定时器
    stopTimer();

    // 释放图片
    freeCurrentImage();

    // 刷新
    update();

    // 改变状态
    setState(Stopped);
}

bool yuvplayer::isPlaying() {
    return _state == Playing;
}


yuvplayer::State yuvplayer::getState() {
    return _state;
}

void yuvplayer::setState(State state) {

    if (state == _state) return;

    if (state == Stopped
            || state == Finished) {
        // 让文件读取指针回到文件首部
        _file->seek(0);
    }

    _state = state;

}


void yuvplayer::setYuv(Yuv &yuv){

    _yuv = yuv;

    //关闭上一个文件
    closeFile();

    _file = new QFile(_yuv.filename);

    if (!_file->open(QFile::ReadOnly)) {
        qDebug() << "file open error" << _yuv.filename;
    }

    _interval = 1000 / _yuv.fps;

    _imgSize = av_image_get_buffer_size(_yuv.pixelFormat,_yuv.width,_yuv.height,1);

    //适配尺寸
    //播放器的宽高
    int w = width();
    int h = height();

    //画面的尺寸
    int dx = 0;
    int dy = 0;
    int dw = _yuv.width;
    int dh = _yuv.height;

    //只要画面宽/高有一个大了放不下就要等比缩放
    if (dw > w || dh > h) {

        //画面宽高比 > 播放器宽高比 则说明宽度放不下，要把画面宽度缩到和播放器一致，则上下黑边
        //dw/dh > w/h 为了防止小数出现做运算
        if (dw*h > dh *w) {
            //维持宽高比 计算dw变为w时的高度
            dh = w*dh /dw;
            //后赋值，维持宽高比
            dw = w;

        }else {
            //画面高宽比 > 播放器高宽比 则说明高度放不下，要把画面高度缩到和播放器一致，则左右黑边
            dw = dw*h/dh;
            dh = h;
        }

    }

    //居中
    dx = (w - dw) >> 1;
    dy = (h - dh) >> 1;

    _dstRect = QRect(dx,dy,dw,dh);
}

// 当组件想重绘的时候，就会调用这个函数
// 想要绘制什么内容，在这个函数中实现
void yuvplayer::paintEvent(QPaintEvent *event) {


    if (!_currentImage) return;

    // 将图片绘制到当前组件上
    QPainter(this).drawImage(_dstRect, *_currentImage);

}

void yuvplayer::timerEvent(QTimerEvent *event) {

    char data[_imgSize];

    if (_file->read(data,_imgSize) == _imgSize) {

        //QImage只接收RGB24格式，所以要把yuv像素格式转换
        //开始转换这一帧
        RawVideoFrame in = {

            data,_yuv.width, _yuv.height,
            _yuv.pixelFormat

        };


        RawVideoFrame out = {
            nullptr,
            //FFMPEG建议像素格式转换后的尺寸最好是16的倍数，不然会损失速度，有其他问题。因此先/16（除不尽损失一点小数） 再 *16就得到一个最接近原尺寸且是16倍数的尺寸。
            _yuv.width >> 4 << 4,
            _yuv.height >> 4 << 4,
            AV_PIX_FMT_RGB24
        };
        ffmpegs::convertRawVideo(in,out);


        freeCurrentImage();

        _currentImage = new QImage((uchar *)out.pixels,out.width,out.height,QImage::Format_RGB888);

        // 刷新
        update();


    } else {

        // 文件数据已经读取完毕
        // 停止定时器
        stopTimer();

        // 正常播放完毕
        setState(Finished);
    }

}


void yuvplayer::freeCurrentImage() {

    if(!_currentImage) return;

    free(_currentImage->bits());

    delete _currentImage;

    _currentImage = nullptr;

}

void yuvplayer::stopTimer() {
    if (_timerId == 0) return;

    killTimer(_timerId);
    _timerId = 0;
}

void yuvplayer::closeFile() {
    if (!_file) return;

    _file->close();
    delete _file;
    _file = nullptr;
}

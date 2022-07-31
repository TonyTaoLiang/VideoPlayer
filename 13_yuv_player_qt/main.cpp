#include "mainwindow.h"

#include <QApplication>

#include "ffmpegs.h"

int main(int argc, char *argv[])
{
//    RawVideoFile in = {

//        "/Users/tonytaoliang/Downloads/pingpong.yuv",
//        320,240,AV_PIX_FMT_YUV420P

//    };

//    RawVideoFile out = {

//        "/Users/tonytaoliang/Downloads/pp.rgb",
//        320,240,AV_PIX_FMT_RGB24

//    };

//    ffmpegs::convertRawVideo(in,out);

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}

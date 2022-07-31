#include "mainwindow.h"

#include <QApplication>
extern "C" {
#include <libavdevice/avdevice.h>
}
#include <iostream>


//void log() {
//    //C++打印
//    std::cout << "stdlog";

//    //FFMpeg打印
//    //日志级别（优先级 > AV_LOG_DEBUG的才能打印出来）
//    //TRACE < DEBUG < INFO < WARNING < ERROR < FATAL < QUIET
//    av_log_set_level(AV_LOG_DEBUG);
//    av_log(nullptr,AV_LOG_ERROR,"AV_LOG_ERROR-----");
//    av_log(nullptr,AV_LOG_WARNING,"AV_LOG_WARNING-----");

//    //刷新标准输出流（有时候控制台打印不出来就需要刷新）
//    fflush(stderr);
//    fflush(stdout);

//    //Qdebug打印
//    qDebug() << "QDEBUG";
//}

int main(int argc, char *argv[])
{
    //注册设备
    avdevice_register_all();

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}

#include "mainwindow.h"

#include <QApplication>

//QT中不能使用C++标准库cout来打印信息
//#include <iostream>

#include <QDebug>//>

//FFmpeg是纯C语言的
//C++是不能直接导入C语言函数的
extern "C" {

    #include <libavcodec/avcodec.h>

}


int main(int argc, char *argv[])
{
    //QT中不能使用标准库打印
//    std::cout << av_version_info();
    // 打印版本号
    qDebug() << av_version_info();
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}

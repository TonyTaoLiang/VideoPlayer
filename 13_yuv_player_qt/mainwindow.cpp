#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "yuvplayer.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 创建播放器
    _player = new yuvplayer(this);

    // 设置播放器的位置和尺寸
    int w = 400;
    int h = 200;
    int x = (width() - w) >> 1;
    int y = (height() - h) >> 1;
    _player->setGeometry(x, y, w, h);

    Yuv yuv = {

        "/Users/tonytaoliang/Downloads/pingpong.yuv",
        320,240,
        AV_PIX_FMT_YUV420P,
        30

    };

    _player->setYuv(yuv);
    _player->play();

}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_playPCM_clicked()
{

}


void MainWindow::on_stopButton_clicked()
{

}


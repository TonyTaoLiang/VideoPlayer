#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_playPCM_clicked()
{
    if(!_playThread) {

        _playThread = new playThread(this);
        _playThread->start();
        ui->playPCM->setText("停止播放");
        //顺利播放结束了
        connect(_playThread,&playThread::finished,[this](){

            ui->playPCM->setText("播放PCM");
            _playThread = nullptr;

        });

    }else {

        _playThread->requestInterruption();
        ui->playPCM->setText("播放PCM");
        _playThread = nullptr;
    }

}


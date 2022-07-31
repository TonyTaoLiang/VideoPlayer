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

void MainWindow::on_audioButton_clicked()
{
    if (!_audioThread) {
        //开启线程
        _audioThread = new AudioThread(this);
        _audioThread->start();
        ui->audioButton->setText("结束录像");

        //开始录音后如何异常退出，改变UI，销毁线程
        //lambda如果要使用外面变量需要捕捉，ui，和_audioThread都是this的,所以捕捉this
        connect(_audioThread,&AudioThread::isFinished,[this](){
            ui->audioButton->setText("开始录像");
            _audioThread = nullptr;
        });

    }else {

        ui->audioButton->setText("开始录像");
        //结束线程
        _audioThread->requestInterruption();
        _audioThread = nullptr;
    }

}

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //初始化时间
    time_changed(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::time_changed(unsigned long long ms) {

    QTime time(0, 0, 0, 0);
    QString text = time.addMSecs(ms).toString("mm:ss.z");
    ui->timelabel->setText(text.left(7));

}


void MainWindow::on_audioButton_clicked()
{
    if (!_audioThread) {
        //开启线程
        _audioThread = new AudioThread(this);
        _audioThread->start();
        ui->audioButton->setText("结束录音");

        //记录时间
        connect(_audioThread,&AudioThread::recordTime,this,&MainWindow::time_changed);

        //开始录音后如何异常退出，改变UI，销毁线程
        //lambda如果要使用外面变量需要捕捉，ui，和_audioThread都是this的,所以捕捉this
        connect(_audioThread,&AudioThread::finished,[this](){
            ui->audioButton->setText("开始录音");
            _audioThread = nullptr;
        });

    }else {

        ui->audioButton->setText("开始录音");
        //结束线程
//        _audioThread->setStop(true);
        _audioThread->requestInterruption();
        _audioThread = nullptr;
    }

}

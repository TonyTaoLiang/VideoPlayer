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


void MainWindow::on_recordBtn_clicked()
{

    if(!_audioThread) {

        _audioThread = new AudioThread(this);

        _audioThread->start();

        this->ui->recordBtn->setText("EndRecord");

    }else {

        _audioThread->requestInterruption();

        _audioThread = nullptr;

        this->ui->recordBtn->setText("StartRecord");
    }
}


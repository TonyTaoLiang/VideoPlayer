#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 注册信号的参数类型，保证能够发出信号
    qRegisterMetaType<videoPlayer::VideoSwsSpec>("VideoSwsSpec&");

    _player = new videoPlayer();

    connect(_player,&videoPlayer::stateChanged,this,&MainWindow::onPlayerStateChanged);
    connect(_player,&videoPlayer::initFinished,this,&MainWindow::onPlayerInitFinished);
    connect(_player, &videoPlayer::playFailed,this, &MainWindow::onPlayerPlayFailed);
    connect(_player, &videoPlayer::timeChanged,this, &MainWindow::onPlayerTimeChanged);
    connect(_player, &videoPlayer::frameDecoded,ui->videoWidget, &VideoWidget::onPlayerFrameDecoded);
    connect(_player, &videoPlayer::stateChanged,ui->videoWidget, &VideoWidget::onPlayerStateChanged);
    // 监听时间滑块的点击
    connect(ui->currentSlider, &videoSlider::clicked,this, &MainWindow::onSliderClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete _player;
}


void MainWindow::on_stopBtn_clicked()
{
    _player->stop();
}


void MainWindow::on_playBtn_clicked()
{
    videoPlayer::State state = _player->getState();
    if (state == videoPlayer::Playing) {
        _player->pause();
    } else {
        _player->play();
    }
}


void MainWindow::on_silenceBtn_clicked()
{
    if (_player->isMute()) {
        _player->setMute(false);
        ui->silenceBtn->setText("静音");
    } else {
        _player->setMute(true);
        ui->silenceBtn->setText("开音");
    }
}


void MainWindow::on_openFileBtn_clicked()
{
    QString filename = QFileDialog::getOpenFileName(nullptr,
                                                    "选择多媒体文件",
                                                    "/Users/tonytaoliang/Downloads",
                                                    "多媒体文件 (*.mp4 *.avi *.mkv *.mp3 *.aac)");
    if (filename.isEmpty()) return;

    // 开始播放打开的文件
    _player->setFilename(filename);
    _player->play();
}

void MainWindow::onSliderClicked(videoSlider *slider) {
    _player->setTime(slider->value());
}

void MainWindow::onPlayerTimeChanged(videoPlayer *player) {
    ui->currentSlider->setValue(player->getTime());
}

void MainWindow::onPlayerStateChanged(videoPlayer *player) {

    videoPlayer::State state = player->getState();

    qDebug() << "main state" <<state;

    if (state == videoPlayer::Playing) {
        ui->playBtn->setText("暂停");
    } else {
        ui->playBtn->setText("播放");
    }

    if (state == videoPlayer::Stopped) {
        ui->playBtn->setEnabled(false);
        ui->stopBtn->setEnabled(false);
        ui->currentSlider->setEnabled(false);
        ui->volumnSlider->setEnabled(false);
        ui->silenceBtn->setEnabled(false);

        ui->durationLabel->setText(getTimeText(0));
        ui->currentSlider->setValue(0);

        // 显示打开文件的页面 有个bug，设置setCurrentWidget 报错，设置index才可以
        //QStackedWidget::setCurrentWidget: widget 0xffffffff not contained in stack
//        ui->playWidget->setCurrentWidget(ui->openFileBtn);
        ui->playWidget->setCurrentIndex(0);
    } else {
        ui->playBtn->setEnabled(true);
        ui->stopBtn->setEnabled(true);
        ui->currentSlider->setEnabled(true);
        ui->volumnSlider->setEnabled(true);
        ui->silenceBtn->setEnabled(true);

        // 显示播放视频的页面
        qDebug() << "显示播放视频的页面";
//        ui->playWidget->setCurrentWidget(ui->videoWidget);
        ui->playWidget->setCurrentIndex(1);
    }
}

void MainWindow::onPlayerInitFinished(videoPlayer *player) {

    //获取视频时长(秒)
    int duration = player->getDuration();

    qDebug() << "视频时长" << duration;
    // 设置slider的范围
    ui->currentSlider->setRange(0, duration);

    // 设置label的文字
    ui->durationLabel->setText(getTimeText(duration));

}

void MainWindow::on_volumnSlider_valueChanged(int value)
{
    ui->volumnLabel->setText(QString("%1").arg(value));
    _player->setVolumn(value);
}


void MainWindow::on_currentSlider_valueChanged(int value)
{
    ui->currentLabel->setText(getTimeText(value));
}

QString MainWindow::getTimeText(int value) {
    //    int h = seconds / 3600;
    //    int m = (seconds % 3600) / 60;
    //    int m = (seconds / 60) % 60;
    //    int s = seconds % 60;


    //    QString h = QString("0%1").arg(seconds / 3600).right(2);
    //    QString m = QString("0%1").arg((seconds / 60) % 60).right(2);
    //    QString s = QString("0%1").arg(seconds % 60).right(2);
    //    return QString("%1:%2:%3").arg(h).arg(m).arg(s);

    QLatin1Char fill = QLatin1Char('0');
    return QString("%1:%2:%3")
            .arg(value / 3600, 2, 10, fill)
            .arg((value / 60) % 60, 2, 10, fill)
            .arg(value % 60, 2, 10, fill);
}

void MainWindow::onPlayerPlayFailed(videoPlayer *player) {
    QMessageBox::critical(nullptr, "提示", "哦豁，播放失败！");
}

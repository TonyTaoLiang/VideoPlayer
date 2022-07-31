#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "videoplayer.h"
#include "videoslider.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_stopBtn_clicked();
    void on_playBtn_clicked();
    void on_silenceBtn_clicked();
    void on_openFileBtn_clicked();
    void on_volumnSlider_valueChanged(int value);
    void on_currentSlider_valueChanged(int value);
    void onPlayerTimeChanged(videoPlayer *player);
    void onSliderClicked(videoSlider *slider);

private:
    Ui::MainWindow *ui;
    videoPlayer *_player;

    void onPlayerStateChanged(videoPlayer *player);

    void onPlayerInitFinished(videoPlayer *player);

    void onPlayerPlayFailed(videoPlayer *player);

    QString getTimeText(int value);
};
#endif // MAINWINDOW_H

#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include "videoplayer.h"
#include <QWidget>

class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

public slots:
    void onPlayerFrameDecoded(videoPlayer *player,
                              uint8_t *data,
                              videoPlayer::VideoSwsSpec &spec);
    void onPlayerStateChanged(videoPlayer *player);

private:
    QImage *_image = nullptr;
    QRect _rect;
    void paintEvent(QPaintEvent *event) override;

    void freeImage();
signals:

};

#endif // VIDEOWIDGET_H

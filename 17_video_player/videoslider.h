#ifndef VIDEOSLIDER_H
#define VIDEOSLIDER_H

#include <QWidget>
#include <QSlider>

class videoSlider : public QSlider
{
    Q_OBJECT
public:
    explicit videoSlider(QWidget *parent = nullptr);

signals:
    /** 点击事件 */
    void clicked(videoSlider *slider);

private:
    void mousePressEvent(QMouseEvent *ev) override;

};

#endif // VIDEOSLIDER_H

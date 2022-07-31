#include "videoslider.h"
#include <QMouseEvent>
#include <QStyle>

videoSlider::videoSlider(QWidget *parent)
    : QSlider{parent}
{

}

void videoSlider::mousePressEvent(QMouseEvent *ev) {

    QStyle::sliderValueFromPosition(minimum(),
                                    maximum(),
                                    ev->pos().x(),
                                    width());

    QSlider::mousePressEvent(ev);

    // 发出信号
    emit clicked(this);

}

#include "receiver.h"
#include <QDebug>

Receiver::Receiver(QObject *parent)
    : QObject{parent}
{

}

int Receiver::handelExit(int a, int b) {

    qDebug() << "AlreadyHandelExit" << a << b;

    return a + b;
}

void Receiver::handelExit2() {

    qDebug() << "AlreadyHandelExit2";

}

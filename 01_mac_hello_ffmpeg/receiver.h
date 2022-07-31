#ifndef RECEIVER_H
#define RECEIVER_H

#include <QObject>

class Receiver : public QObject
{
    Q_OBJECT
public:
    explicit Receiver(QObject *parent = nullptr);
//注意public
public slots:
    int handelExit(int a, int b);
    void handelExit2();
};

#endif // RECEIVER_H

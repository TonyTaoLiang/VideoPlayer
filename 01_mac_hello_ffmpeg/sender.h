#ifndef SENDER_H
#define SENDER_H

#include <QObject>

class Sender : public QObject
{
    Q_OBJECT
public:
    explicit Sender(QObject *parent = nullptr);

signals:
    int exit(int a, int b);
    void exit2();
    void exit3();
};

#endif // SENDER_H

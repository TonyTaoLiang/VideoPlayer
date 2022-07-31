#ifndef PLAYTHREAD_H
#define PLAYTHREAD_H

#include <QObject>
#include <QThread>

class playThread : public QThread
{
    Q_OBJECT
public:
    explicit playThread(QObject *parent = nullptr);
    ~playThread();

private:
    void run();
signals:

};

#endif // PLAYTHREAD_H

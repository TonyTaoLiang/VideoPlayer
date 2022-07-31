#ifndef AUDIOTHREAD_H
#define AUDIOTHREAD_H

#include <QThread>

class AudioThread : public QThread
{
    Q_OBJECT
public:
    explicit AudioThread(QObject *parent = nullptr);
    ~AudioThread();
    void setStop(bool stop);
private:

    bool _stop = false;
    void run() override;

signals:
    void recordTime(unsigned long long ms);
};

#endif // AUDIOTHREAD_H

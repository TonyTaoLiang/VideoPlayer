#include "ffmpegs.h"
#include <QFile>
#include <QDebug>

FFMpegs::FFMpegs()
{

}

void FFMpegs::pcm2wav(WAVHead &head,
                    const char *pcmfileName,
                    const char *wavfileName){

    head.blockAlign = head.bitsPerSample*head.numChannels >> 3;
    head.byteRate = head.sampleRate * head.blockAlign;

    QFile wavFile = QFile(wavfileName);

    if(!wavFile.open(QFile::WriteOnly)){

        qDebug() << "打开wav文件失败";
        return;
    }

    //不能在这写入，要在后面文件全部打开在写入，不然有问题。
    //因为在后面才设置的head.dataChunkSize，head.riffChunkSize
//    wavFile.write((char *)&head,sizeof(head));

    QFile pcmFile = QFile(pcmfileName);
    if(!pcmFile.open(QFile::ReadOnly)){

        qDebug() << "打开PCM文件失败";
        wavFile.close();
        return;
    }

    head.dataChunkSize = pcmFile.size();
    //PCM数据大小 + 44字节的head - riffChunkID - riffChunkSize
    head.riffChunkSize = head.dataChunkSize
                               + sizeof (WAVHead)
                               - sizeof (head.riffChunkID)
                               - sizeof (head.riffChunkSize);

    wavFile.write((char *)&head,sizeof(head));

    char buff[1024];
    int size = 0;

    while((size = pcmFile.read(buff,sizeof(buff))) > 0){
        wavFile.write(buff,size);
    }

    pcmFile.close();
    wavFile.close();

}

#include "ffmpegs.h"
#include <QDebug>
#include <QFile>
#include <QObject>

extern "C" {

#include <libswresample/swresample.h>
#include <libavutil/avutil.h>

}

#define ERRORBUF(ret) \
    char errbuf[1024]; \
    av_strerror(ret,errbuf,sizeof(errbuf));


FFmpegs::FFmpegs()
{

}

void FFmpegs::audioResample(ResampleAudioSpec &in,
                            ResampleAudioSpec &out) {

    audioResample(in.fileName,
                  in.ch_layout,
                  in.sampleFormat,
                  in.sampleRate,
                  out.fileName,
                  out.ch_layout,
                  out.sampleFormat,
                  out.sampleRate
                  );


}

void FFmpegs::audioResample(const char* infileName,
                            int64_t in_ch_layout,
                            AVSampleFormat informat,
                            int insampleRate,
                            const char* outfileName,
                            int64_t out_ch_layout,
                            AVSampleFormat outformat,
                            int outsampleRate
                            ) {

    //都先在前面定义，防止前面一步goto end，后面有的变量压根就没创建而报错
    QFile infile = QFile(infileName);
    //读取文件的大小
    int readSize = 0;

    QFile outfile = QFile(outfileName);

    int ret = 0;

    //指向输入缓冲区的指针
    uint8_t **in_audio_data = nullptr;
    //缓冲区大小：给0是因为函数需要传它的地址，那么函数调用完，这个就有值了，也就告诉你分配了一块多大的内存
    int in_linesize = 0;
    //声道数
    int in_channels = av_get_channel_layout_nb_channels(in_ch_layout);
    //缓冲区的样本数number of samples per channel
    int in_nb_samples = 1024;
    //buffer size alignment (0 = default, 1 = no alignment)
    int in_align = 1;
    //一个样本的大小
    int inBytesPersample = in_channels * av_get_bytes_per_sample(informat);

    //指向输出缓冲区的指针
    uint8_t **out_audio_data = nullptr;
    //缓冲区大小：给0是因为函数需要传它的地址，那么函数调用完，这个就有值了，也就告诉你分配了一块多大的内存
    int out_linesize = 0;
    //声道数
    int out_channels = av_get_channel_layout_nb_channels(out_ch_layout);

    //缓冲区的样本数number of samples per channel
    //int out_nb_samples = 1024;
    //重采样歌曲的时长是不变的，采样率不同则导致输入/输出缓冲区的样本数量应该不同
    //inSampleRate / outSampleRate = inSamples /  outSamples 向上取整
    int out_nb_samples = av_rescale_rnd(in_nb_samples,outsampleRate,insampleRate,AV_ROUND_UP);

    //buffer size alignment (0 = default, 1 = no alignment)
    int out_align = 1;
    //一个样本的大小
    int outBytesPersample = out_channels * av_get_bytes_per_sample(outformat);

    //创建重采样上下文
    SwrContext *ctx = swr_alloc_set_opts(nullptr,
                                         out_ch_layout,outformat,outsampleRate,
                                         in_ch_layout,informat,insampleRate,
                                         0,nullptr);

    if(!ctx){

        qDebug() << "swr_alloc_set_opts error";
        goto end;
    }

    //初始化上下文
    ret = swr_init(ctx);

    if(ret < 0) {
        ERRORBUF(ret);
        qDebug() << "swr_init error" << errbuf;
        goto end;
    }

    //创建输入缓冲区
    ret = av_samples_alloc_array_and_samples(&in_audio_data,
                                             &in_linesize,
                                             in_channels,
                                             in_nb_samples,
                                             informat,
                                             in_align);
    if(ret < 0) {
        ERRORBUF(ret);
        qDebug() << "av_samples_alloc_array_and_samples in error" << errbuf;
        goto end;
    }

    //创建输出缓冲区
    ret = av_samples_alloc_array_and_samples(&out_audio_data,
                                             &out_linesize,
                                             out_channels,
                                             out_nb_samples,
                                             outformat,
                                             out_align);

    if(ret < 0) {
        ERRORBUF(ret);
        qDebug() << "av_samples_alloc_array_and_samples out error" << errbuf;
        goto end;
    }

    //开始转换
    if(!infile.open(QFile::ReadOnly)) {

        qDebug() << "infile文件打开失败";
        goto end;
    }

    if(!outfile.open(QFile::WriteOnly)) {

        qDebug() << "outfile文件打开失败";
        goto end;
    }


    while ((readSize = infile.read((char *)in_audio_data[0],in_linesize)) > 0) {

        //由readSize计算出实际上读取缓冲区样本的数量(不是每一次都是读取的1024)
        in_nb_samples = readSize / inBytesPersample;
        //返回输出的样本数量 number of samples output per channel
        ret = swr_convert(ctx,
                    out_audio_data,
                    out_nb_samples,
                    (const uint8_t **)in_audio_data,
                    in_nb_samples);

        if(ret < 0) {

            ERRORBUF(ret);
            qDebug() << "swr_convert error" << errbuf;
            goto end;
        }

        //写入文件(返回的输出样本数量*每个输出样本的大小=总大小)
        outfile.write((char *)out_audio_data[0],
                      ret * outBytesPersample);
    }

    //需要检查输出缓冲区是否还有残留的样本（已经重采样过的，转换过的）, 将其flush刷出去
    //in and in_count can be set to 0 to flush the last few samples out at the end
    while ((ret = swr_convert(ctx,out_audio_data,out_nb_samples,0,0)) > 0){

        outfile.write((char *)out_audio_data[0],
                      ret * outBytesPersample);

    }

    //释放
end:

    infile.close();
    outfile.close();
    //释放输入缓冲区
    if(in_audio_data) {
        //先释放uint8_t的堆空间，传入&的目的，是为了清空这个指针，如in_audio_data[0] = nullptr
        av_freep(&in_audio_data[0]);
    }
    //再释放uint8_t*的堆空间
    av_freep(&in_audio_data);

    if(out_audio_data) {
        av_freep(&out_audio_data[0]);
    }
    av_freep(&out_audio_data);
    swr_free(&ctx);


}

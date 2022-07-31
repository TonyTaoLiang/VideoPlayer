#include "playthread.h"
#include <QDebug>
#include <SDL2/SDL.h>
#include <QFile>

#define END(judge, func) \
    if (judge) { \
        qDebug() << #func << "Error" << SDL_GetError(); \
        goto end; \
    }

playThread::playThread(QObject *parent)
    : QThread{parent}
{
    connect(this,&playThread::finished,this,&playThread::deleteLater);
}

playThread::~playThread(){
    disconnect();
    requestInterruption();
    quit();
    wait();
    qDebug() << this << "析构函数";
}

void playThread::run() {

    // 像素数据
    SDL_Surface *surface = nullptr;

    //窗口
    SDL_Window *window = nullptr;

    //渲染上下文
    SDL_Renderer *render = nullptr;

    //纹理（直接跟特定驱动程序相关的像素数据）
    SDL_Texture *texture = nullptr;

    // 矩形框


    // 初始化子系统 SDL_INIT_VIDEO 直接崩溃 不懂为什么？？？不能在子线程渲染！
    END(SDL_Init(SDL_INIT_VIDEO),SDL_Init);


    // 加载BMP
    surface = SDL_LoadBMP("/Users/tonytaoliang/Downloads/00011.bmp");
    END(!surface,SDL_LoadBMP);

    // 创建窗口
    window = SDL_CreateWindow("SDL显示BMP图片",
                     SDL_WINDOWPOS_UNDEFINED,
                     SDL_WINDOWPOS_UNDEFINED,
                     surface->w,
                     surface->h,
                     SDL_WINDOW_SHOWN);
    END(!window, SDL_CreateWindow);

    // 创建渲染上下文
    render = SDL_CreateRenderer(window,
                                -1,
                                SDL_RENDERER_ACCELERATED |
                                SDL_RENDERER_PRESENTVSYNC);
    if (!render) { // 说明开启硬件加速失败.看源码就是这样写的
        render = SDL_CreateRenderer(window, -1, 0);
        END(!render, SDL_CreateRenderer);
    }

    //创建纹理
    texture = SDL_CreateTextureFromSurface(render,surface);
    END(!texture, SDL_CreateTextureFromSurface);

    // 设置绘制颜色（画笔颜色）
    END(SDL_SetRenderDrawColor(render,
                               255, 255, 0, SDL_ALPHA_OPAQUE),
        SDL_SetRenderDrawColor);

    // 用绘制颜色（画笔颜色）清除渲染目标。（清空画布）
    END(SDL_RenderClear(render),SDL_RenderClear);

    //拷贝纹理数据到渲染目标（默认是window）
    END(SDL_RenderCopy(render,texture,nullptr,nullptr),SDL_RenderCopy);

    // 更新所有的渲染操作到屏幕上
    SDL_RenderPresent(render);

    //等待退出事件
    while (!isInterruptionRequested()) {

        SDL_Event event;
        SDL_WaitEvent(&event);

        switch (event.type) {
            case SDL_QUIT:
            goto end;
        }

    }

end:
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(render);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

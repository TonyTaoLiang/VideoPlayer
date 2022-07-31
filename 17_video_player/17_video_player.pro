QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    condmutex.cpp \
    main.cpp \
    mainwindow.cpp \
    videoPlayer_audio.cpp \
    videoPlayer_video.cpp \
    videoplayer.cpp \
    videoslider.cpp \
    videowidget.cpp

HEADERS += \
    condmutex.h \
    mainwindow.h \
    videoplayer.h \
    videoslider.h \
    videowidget.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

mac: {

FFMPEG_HOME = /usr/local/ffmpeg

# 头文件路径
INCLUDEPATH += $${FFMPEG_HOME}/include

# 库文件路径
# 需要链接哪些库 默认链接动态库

LIBS += -L $${FFMPEG_HOME}/lib \
    -lavformat \
    -lavutil \
    -lavcodec \
    -lswresample \
    -lswscale

SDL_HOME = /usr/local/Cellar/sdl2/2.0.20

# 头文件路径
INCLUDEPATH += $${SDL_HOME}/include

# 库文件路径
# 需要链接哪些库 默认链接动态库
LIBS += -L$${SDL_HOME}/lib -lSDL2
}

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    receiver.cpp \
    sender.cpp

HEADERS += \
    mainwindow.h \
    receiver.h \
    sender.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# 头文件路径
#INCLUDEPATH += /usr/local/Cellar/ffmpeg/5.0.1/include

## 库文件路径
## 需要链接哪些库 默认链接动态库
#LIBS += -L /usr/local/Cellar/ffmpeg/5.0.1/lib \
#        -lavcodec \
#        -lavdevice \
#        -lavfilter \
#        -lavformat \
#        -lavutil \
#        -lpostproc \
#        -lswscale \
#        -lswresample

mac: {

    FFMPEG_HOME = /usr/local/ffmpeg

    # 打印mac环境变量 PATH 注意系统使用小括号 在调试栏（6概要信息）
    message($$(PATH))

    message($${FFMPEG_HOME})

    # 头文件路径
    INCLUDEPATH += $${FFMPEG_HOME}/include

    # 库文件路径
    # 需要链接哪些库 默认链接动态库
    LIBS += -L $${FFMPEG_HOME}/lib \
        -lavcodec \
        -lavdevice \
        -lavfilter \
        -lavformat \
        -lavutil \
        -lpostproc \
        -lswscale \
        -lswresample

}

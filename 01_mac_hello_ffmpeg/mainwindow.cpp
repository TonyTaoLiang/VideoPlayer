#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QPushButton>
#include "sender.h"
#include "receiver.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("主窗口");

    setFixedSize(600,600);

    QPushButton *btn = new QPushButton;
    btn->setText("关闭");
    btn->setFixedSize(100,30);
    btn->setParent(this);
    btn->move(100,200);

    new QPushButton("注册",this);

    //QT自动销毁了,btn是window的子控件，window销毁btn销毁
//    delete btn;

    //1.信号与槽1:原生方法
    connect(btn,&QAbstractButton::clicked,this,&MainWindow::close);

    //2.信号与槽2：自定义信号与槽
    Sender *sender = new Sender;
    Receiver *receiver = new Receiver;
    //连接信号与槽
    connect(sender,&Sender::exit,receiver,&Receiver::handelExit);
    //发出信号
    //qDebug() << emit sender->exit(10,20);

    //3.连接2个信号：信号1+信号2.那么发送信号1时会同时触发2个槽函数
    //先连接第二个信号和第二个槽
    connect(sender,&Sender::exit2,receiver,&Receiver::handelExit2);
    //再连接2个信号
    connect(sender,&Sender::exit,sender,&Sender::exit2);
    //发送信号1，同时也触发了信号2
    qDebug() << emit sender->exit(10,20);

    //4.lambda,类似block [] 表示lambda值捕获的
    connect(sender,&Sender::exit3,[](){
        qDebug() << "lambda";
    });

    emit sender->exit3();
    //释放
    delete sender;
    delete receiver;

}

MainWindow::~MainWindow()
{
    delete ui;
}


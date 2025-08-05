#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include "global.h"
#include <QStyleFactory>
#include <QPalette>
#include "tcpmgr.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 使用Fusion样式
    a.setStyle(QStyleFactory::create("Fusion"));

    // 配置浅色调色板
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(240, 240, 240));      // 窗口背景
    palette.setColor(QPalette::WindowText, Qt::black);              // 文本颜色
    palette.setColor(QPalette::Base, QColor(255, 255, 255));        // 输入控件背景
    palette.setColor(QPalette::AlternateBase, QColor(233, 233, 233));
    palette.setColor(QPalette::Text, Qt::black);                    // 输入文本颜色
    palette.setColor(QPalette::Button, QColor(240, 240, 240));      // 按钮背景
    palette.setColor(QPalette::ButtonText, Qt::black);              // 按钮文本
    palette.setColor(QPalette::Highlight, QColor(0, 120, 215));     // 选中高亮
    palette.setColor(QPalette::HighlightedText, Qt::white);

    a.setPalette(palette);


    QFile qss(":/style/stylesheet.qss");

    if( qss.open(QFile::ReadOnly))
    {
        qDebug("open success");
        QString style = QLatin1String(qss.readAll());
        a.setStyleSheet(style);
        qss.close();
    }else{
         qDebug("Open failed");
     }


    // 获取当前应用程序的路径
    QString app_path = QCoreApplication::applicationDirPath();
    // 拼接文件名
    QString fileName = "config.ini";
    QString config_path = QDir::toNativeSeparators(app_path +
                             QDir::separator() + fileName);

    QSettings settings(config_path, QSettings::IniFormat);
    QString gate_host = settings.value("GateServer/host").toString();
    QString gate_port = settings.value("GateServer/port").toString();
    gate_url_prefix = "http://"+gate_host+":"+gate_port;

    qDebug()<< "gate_url_prefix: " << gate_url_prefix ;

    TcpThread tcpthread;
    MainWindow w;
    w.show();
    return a.exec();
}

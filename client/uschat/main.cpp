#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QStyleFactory>
#include <QPalette>

#include "global.h"
#include "tcpmgr.h"
#include "filetcpmgr.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);


    // 禁用 Qt 6 的系统主题自动检测
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

// 如果是 Windows，强制不使用深色标题栏
#ifdef Q_OS_WIN
    qputenv("QT_QPA_PLATFORM", "windows:darkmode=0");
#endif

    // 1. 强制使用融合风格（Fusion），它在各平台表现最一致
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // 2. 强制设置浅色调色板
    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::WindowText, Qt::black);
    lightPalette.setColor(QPalette::Base, Qt::white);
    lightPalette.setColor(QPalette::AlternateBase, QColor(233, 233, 233));
    lightPalette.setColor(QPalette::ToolTipBase, Qt::white);
    lightPalette.setColor(QPalette::ToolTipText, Qt::black);
    lightPalette.setColor(QPalette::Text, Qt::black);
    lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::ButtonText, Qt::black);
    lightPalette.setColor(QPalette::BrightText, Qt::red);
    lightPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    lightPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    lightPalette.setColor(QPalette::HighlightedText, Qt::white);

    QApplication::setPalette(lightPalette);



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

    //启动tcp线程
    TcpThread tcpthread;
    //启动资源网络线程
    FileTcpThread file_tcp_thread;
    MainWindow w;
    w.show();
    return a.exec();
}

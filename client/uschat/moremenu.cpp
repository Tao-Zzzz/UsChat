#include "moremenu.h"
#include <QGraphicsDropShadowEffect>


// MoreMenu::MoreMenu(QWidget *parent) : QWidget(parent) {
//     setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
//     setAttribute(Qt::WA_TranslucentBackground); // 關鍵：支持透明陰影

//     auto* layout = new QVBoxLayout(this);
//     layout->setContentsMargins(10, 10, 10, 10);

//     // 容器，用於繪製陰影和圓角
//     QWidget* container = new QWidget(this);
//     container->setObjectName("MenuContainer");
//     QVBoxLayout* c_layout = new QVBoxLayout(container);
//     c_layout->setContentsMargins(0, 5, 0, 5);
//     c_layout->setSpacing(0);

//     auto* btn = new QPushButton(" 加載歷史聊天記錄", this);
//     btn->setFixedSize(160, 35);
//     btn->setCursor(Qt::PointingHandCursor);

//     // QSS 美化
//     container->setStyleSheet(
//         "#MenuContainer { background: white; border: 1px solid #e0e0e0; border-radius: 6px; }"
//         "QPushButton { border: none; background: transparent; text-align: left; padding-left: 15px; font-size: 13px; color: #333; }"
//         "QPushButton:hover { background: #f5f5f5; color: #0078d4; }"
//         );

//     // 添加陰影效果
//     auto* shadow = new QGraphicsDropShadowEffect(this);
//     shadow->setBlurRadius(10);
//     shadow->setColor(QColor(0, 0, 0, 40));
//     shadow->setOffset(0, 2);
//     container->setGraphicsEffect(shadow);

//     c_layout->addWidget(btn);
//     layout->addWidget(container);

//     connect(btn, &QPushButton::clicked, [this](){
//         emit sig_switch_history();
//         this->hide();
//     });
// }

MoreMenu::MoreMenu(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8); // 給陰影留空間

    QWidget* container = new QWidget(this);
    container->setStyleSheet("background: #252525; border: 1px solid #3c3c3c; border-radius: 4px;");

    auto* c_layout = new QVBoxLayout(container);
    c_layout->setContentsMargins(0, 4, 0, 4);

    auto* btn = new QPushButton(" 切换历史聊天记录", this);
    btn->setFixedSize(150, 36);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton { border: none; color: #ccc; text-align: left; padding-left: 12px; font-size: 13px; }"
        "QPushButton:hover { background: #3c3c3c; color: #fff; }"
        );

    // 陰影
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(12);
    shadow->setColor(QColor(0, 0, 0, 80));
    shadow->setOffset(0, 2);
    container->setGraphicsEffect(shadow);

    c_layout->addWidget(btn);
    layout->addWidget(container);

    connect(btn, &QPushButton::clicked, [this](){
        emit sig_switch_history();
        this->hide();
    });
}

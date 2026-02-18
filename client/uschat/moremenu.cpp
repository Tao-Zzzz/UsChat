#include "MoreMenu.h"
#include <QGraphicsDropShadowEffect>
#include <QFrame>
#include <QColor>

MoreMenu::MoreMenu(QWidget *parent)
    : QWidget(parent)
{
    // 窗口属性
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    // 主布局（给阴影留空间）
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    m_mainLayout->setSpacing(0);

    // 容器
    m_container = new QWidget(this);
    m_container->setStyleSheet(
        "background: #ffffff;"
        "border: 1px solid #dcdcdc;"
        "border-radius: 6px;"
        );

    // 阴影
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(18);
    shadow->setColor(QColor(0, 0, 0, 80));
    shadow->setOffset(0, 3);
    m_container->setGraphicsEffect(shadow);

    // 容器内部布局
    m_containerLayout = new QVBoxLayout(m_container);
    m_containerLayout->setContentsMargins(0, 6, 0, 6);
    m_containerLayout->setSpacing(0);

    m_mainLayout->addWidget(m_container);

    // 按钮统一样式
    m_buttonStyle =
        "QPushButton {"
        "border: none;"
        "color: #333333;"
        "text-align: left;"
        "padding-left: 14px;"
        "height: 36px;"
        "font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "background: #f2f2f2;"
        "color: #000000;"
        "}";
}

QPushButton* MoreMenu::addMenuItem(const QString& text)
{
    auto* btn = new QPushButton(text, m_container);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(m_buttonStyle);

    m_containerLayout->addWidget(btn);

    // 点击后自动关闭
    connect(btn, &QPushButton::clicked, this, [this]() {
        this->hide();
    });

    return btn;
}

void MoreMenu::addSeparator()
{
    auto* line = new QFrame(m_container);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet("background: #e5e5e5;");
    m_containerLayout->addWidget(line);
}

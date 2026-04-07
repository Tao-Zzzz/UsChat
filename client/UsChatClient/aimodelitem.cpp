#include "AiModelItem.h"
#include <QHBoxLayout>
#include <QMouseEvent>

AiModelItem::AiModelItem(const QString& name,
                         const QPixmap& icon,
                         QWidget* parent)
    : QWidget(parent), _modelName(name)
{
    setFixedHeight(50);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(10);

    // 图标接口（可为空）
    if (!icon.isNull())
    {
        auto* iconLabel = new QLabel(this);
        iconLabel->setPixmap(icon.scaled(24, 24,
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
        layout->addWidget(iconLabel);
    }

    auto* nameLabel = new QLabel(name, this);
    nameLabel->setStyleSheet("color: #000000; font-size: 14px;");

    layout->addWidget(nameLabel);
    layout->addStretch();

    setStyleSheet(R"(
        QWidget {
            background-color: transparent;
        }
        QWidget:hover {
            background-color: #f2f2f2;
        }
    )");
}

void AiModelItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit sig_clicked(_modelName);
    }
    QWidget::mousePressEvent(event);
}

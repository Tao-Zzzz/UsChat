#include "aihistoryitem.h"
#include "AiHistoryItem.h"
#include <QMouseEvent>

AiHistoryItem::AiHistoryItem(const QString& name,
                             const QString& date,
                             QWidget* parent)
    : QWidget(parent), _name(name)
{
    setFixedHeight(48);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);

    auto* nameLabel = new QLabel(name, this);
    auto* dateLabel = new QLabel(date, this);
    dateLabel->setStyleSheet("color: gray;");

    layout->addWidget(nameLabel);
    layout->addStretch();
    layout->addWidget(dateLabel);

    setStyleSheet(R"(
        QWidget {
            background-color: transparent;
        }
        QLabel {
            color: #000000;
        }
    )");
}

void AiHistoryItem::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        emit sig_clicked(_name);
    }
    QWidget::mousePressEvent(event);
}

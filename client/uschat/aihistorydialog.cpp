#include "AiHistoryDialog.h"
#include "AiHistoryItem.h"

AiHistoryDialog::AiHistoryDialog(QWidget* parent)
    : QDialog(parent)
{
    initUI();
    loadHistoryList();

    setWindowTitle("已归档的聊天");
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setFixedSize(650, 420);
}

void AiHistoryDialog::initUI()
{
    this->setStyleSheet(
        "QDialog { background-color: #ffffff; }"
        "QListWidget { background-color: transparent; border: none; outline: none; }"
        );

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);


    _listWidget = new QListWidget(this);
    _listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    mainLayout->addWidget(_listWidget);
}

void AiHistoryDialog::loadHistoryList()
{
    struct Item {
        QString name;
        QString date;
    };

    QList<Item> items = {
        { "自我成长与突破", "2025年3月18日" },
        { "难以集中注意力", "2025年2月23日" },
        { "口齿不清训练方法", "2025年2月18日" },
        { "迷茫的原因", "2025年2月17日" },
        { "改变未来思维习惯", "2025年2月9日" }
    };

    for(const auto& it : items) {
        auto* item = new QListWidgetItem(_listWidget);
        item->setSizeHint(QSize(0, 48));

        auto* widget = new AiHistoryItem(it.name, it.date, this);
        _listWidget->setItemWidget(item, widget);

        // ⚠️ 关键：一次连接，点击 → 发信号 → 关闭
        connect(widget, &AiHistoryItem::sig_clicked,
                this, [this](const QString& name){
                    emit sig_history_selected(name);
                    accept();   // 正确关闭方式
                });
    }
}

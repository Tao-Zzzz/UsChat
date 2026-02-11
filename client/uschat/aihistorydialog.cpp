#include "AiHistoryDialog.h"
#include "AiHistoryItem.h"
#include "aimgr.h"

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

static QString FormatTime(const QString& isoTime)
{
    QDateTime dt = QDateTime::fromString(isoTime, Qt::ISODate);
    if (!dt.isValid())
        return isoTime;

    return dt.toString("yyyy-MM-dd");
}

void AiHistoryDialog::loadHistoryList()
{
    struct Item {
        QString name;
        QString date;
    };

    auto history_datas = AIMgr::GetInstance()->GetAllAiHistoryChat();

    for (auto it = history_datas.begin(); it != history_datas.end(); ++it) {
        int ai_thread_id = it.key();
        qDebug() << "[Debug]ai_thread_id 是" << ai_thread_id ;
        const auto& data = it.value();

        auto* item = new QListWidgetItem(_listWidget);
        item->setSizeHint(QSize(0, 48));

        // 时间格式化（下面单独解释）
        QString showTime = FormatTime(data->_last_updated_time);

        auto* widget = new AiHistoryItem(data->_title, showTime, this);
        _listWidget->setItemWidget(item, widget);

        connect(widget, &AiHistoryItem::sig_clicked,
                this, [this, ai_thread_id]() {


                    emit sig_history_selected(ai_thread_id);
                    accept();
                });
    }
}


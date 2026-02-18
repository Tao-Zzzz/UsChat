#include "AiModelDialog.h"
#include "AiModelItem.h"
#include <QVBoxLayout>
#include "aimgr.h"

AiModelDialog::AiModelDialog(QWidget* parent)
    : QDialog(parent)
{
    initUI();

    setWindowTitle("选择模型");
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setFixedSize(400, 360);

    setModelList();
}

void AiModelDialog::initUI()
{
    this->setStyleSheet(
        "QDialog { background-color: #ffffff; }"
        "QListWidget { background-color: transparent; border: none; outline: none; }"
        );

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    _listWidget = new QListWidget(this);
    _listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    mainLayout->addWidget(_listWidget);
}

void AiModelDialog::setModelList()
{
    _listWidget->clear();

    auto models = AIMgr::GetInstance()->GetAllAiModel();


    for (auto it = models.constBegin(); it != models.constEnd(); ++it) {
        int model_id  = it.key();
        QString model_name = it.value();

        auto* item = new QListWidgetItem(_listWidget);
        item->setSizeHint(QSize(0, 50));

        // 目前图标留空接口
        auto* widget = new AiModelItem(model_name, QPixmap(), this);

        _listWidget->setItemWidget(item, widget);

        connect(widget, &AiModelItem::sig_clicked,
                this, [this, model_id]() {

                    emit sig_model_selected(model_id);
                    accept();
                });
    }

}

#ifndef AIMODELDIALOG_H
#define AIMODELDIALOG_H

#include <QDialog>
#include <QListWidget>

class AiModelDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AiModelDialog(QWidget* parent = nullptr);

    // 设置模型列表
    void setModelList();

signals:
    void sig_model_selected(int ai_model_id);

private:
    void initUI();

private:
    QListWidget* _listWidget;
};

#endif // AIMODELDIALOG_H

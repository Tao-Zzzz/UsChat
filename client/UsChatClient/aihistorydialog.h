#ifndef AIHISTORYDIALOG_H
#define AIHISTORYDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>

class AiHistoryDialog : public QDialog {
    Q_OBJECT
public:
    explicit AiHistoryDialog(QWidget* parent = nullptr);

signals:
    void sig_history_selected(int ai_thread_id);

private:
    void initUI();
    void loadHistoryList();

private:
    QListWidget* _listWidget;
};


#endif // AIHISTORYDIALOG_H

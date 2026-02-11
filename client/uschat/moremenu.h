#ifndef MOREMENU_H
#define MOREMENU_H

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>

class MoreMenu : public QWidget {
    Q_OBJECT
public:
    explicit MoreMenu(QWidget *parent = nullptr);
signals:
    void sig_switch_history(); // 點擊“加載歷史”信號
protected:
    void leaveEvent(QEvent *event) override { hide(); } // 鼠標離開自動消失
};

#endif // MOREMENU_H

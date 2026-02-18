#ifndef MOREMENU_H
#define MOREMENU_H

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>

class MoreMenu : public QWidget
{
    Q_OBJECT
public:
    explicit MoreMenu(QWidget *parent = nullptr);

    // 添加菜单项，返回按钮指针，外部可自行 connect
    QPushButton* addMenuItem(const QString& text);

    // 添加分割线
    void addSeparator();

signals:
    void sig_switch_history();
    void sig_clear_history();

private:
    QWidget*        m_container;
    QVBoxLayout*    m_mainLayout;
    QVBoxLayout*    m_containerLayout;

    QString m_buttonStyle;
};

#endif // MOREMENU_H

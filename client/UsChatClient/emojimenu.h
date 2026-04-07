#ifndef EMOJIMENU_H
#define EMOJIMENU_H

#include <QFrame>
#include <QVector>
#include <QString>

class QPushButton;

class EmojiMenu : public QFrame
{
    Q_OBJECT
public:
    explicit EmojiMenu(QWidget *parent = nullptr);

signals:
    void sig_emoji_selected(const QString& token);

protected:
    void focusOutEvent(QFocusEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct EmojiItem
    {
        QString token;
        QString iconPath;
        QString tip;
    };

    void initUi();
    void addEmojiButton(const EmojiItem& item, int row, int col);

private:
    QVector<EmojiItem> _emoji_items;
};

#endif // EMOJIMENU_H

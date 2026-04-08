#ifndef CHATVIEW_H
#define CHATVIEW_H

#include <QScrollArea>
#include <QVBoxLayout>
#include <QTimer>
#include <QList>

class ChatView : public QWidget
{
    Q_OBJECT
public:
    ChatView(QWidget *parent = Q_NULLPTR);

    void appendChatItem(QWidget *item);
    void prependChatItem(QWidget *item);
    void insertChatItem(QWidget *before, QWidget *item);
    void removeAllItem();

    QList<QWidget*> takeAllItems();
    void setChatItems(const QList<QWidget*>& items);
    void clearLayoutItemsOnly();

    void beginBatchUpdate();
    void endBatchUpdate(bool scrollToBottom = true);

    void scrollToBottom();

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onVScrollBarMoved(int min, int max);

private:
    void initStyleSheet();

private:
    QVBoxLayout *m_pVl = nullptr;
    QScrollArea *m_pScrollArea = nullptr;
    bool isAppended = false;
    bool m_batchUpdating = false;
};

#endif // CHATVIEW_H

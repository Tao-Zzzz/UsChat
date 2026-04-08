#ifndef TEXTBUBBLE_H
#define TEXTBUBBLE_H

#include <QTextBrowser>
#include "BubbleFrame.h"

class TextBubble : public BubbleFrame
{
    Q_OBJECT
public:
    TextBubble(ChatRole role, const QString &text, QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    void setMessageText(const QString &text);
    void adjustBubbleSize();
    QString convertToHtml(const QString& text) const;
    QString replaceEmojiTokens(QString escapedText) const;
    int maxTextWidth() const;
    void initStyleSheet();

private:
    QTextBrowser *m_pTextEdit;
};

#endif // TEXTBUBBLE_H

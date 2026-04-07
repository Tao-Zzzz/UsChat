#include "TextBubble.h"
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTextOption>
#include <QRegularExpression>
#include <QtMath>

TextBubble::TextBubble(ChatRole role, const QString &text, QWidget *parent)
    : BubbleFrame(role, parent)
{
    m_pTextEdit = new QTextBrowser(this);
    m_pTextEdit->setReadOnly(true);
    m_pTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTextEdit->setFrameShape(QFrame::NoFrame);
    m_pTextEdit->setOpenLinks(false);
    m_pTextEdit->setOpenExternalLinks(false);
    m_pTextEdit->setContextMenuPolicy(Qt::NoContextMenu);
    m_pTextEdit->viewport()->setAutoFillBackground(false);
    m_pTextEdit->document()->setDocumentMargin(0);
    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_pTextEdit->document()->setDefaultTextOption(opt);
    m_pTextEdit->installEventFilter(this);

    m_pTextEdit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_pTextEdit->setFocusPolicy(Qt::StrongFocus);

    QFont font("Microsoft YaHei");
    font.setPointSize(12);
    m_pTextEdit->setFont(font);

    setWidget(m_pTextEdit);
    initStyleSheet();
    setMessageText(text);
}

bool TextBubble::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_pTextEdit && (e->type() == QEvent::Resize || e->type() == QEvent::Show)) {
        adjustBubbleSize();
    }
    return BubbleFrame::eventFilter(o, e);
}

int TextBubble::maxTextWidth() const
{
    return 260;
}

QString TextBubble::replaceEmojiTokens(QString escapedText) const
{
    static const QMap<QString, QString> emojiMap = {
        {"[yd_smile]", ":/res/emoji/yd_smile.png"},
        {"[yd_happy]", ":/res/emoji/yd_happy.png"},
        {"[yd_love]",  ":/res/emoji/yd_love.png"},
        {"[yd_shy]",   ":/res/emoji/yd_shy.png"},
        {"[yd_cry]",   ":/res/emoji/yd_cry.png"},
        {"[yd_angry]", ":/res/emoji/yd_angry.png"},
        {"[yd_sleep]", ":/res/emoji/yd_sleep.png"},
        {"[yd_ok]",    ":/res/emoji/yd_ok.png"},
        {"[yd_wow]",   ":/res/emoji/yd_wow.png"},
        {"[yd_sweat]", ":/res/emoji/yd_sweat.png"},
        {"[yd_think]", ":/res/emoji/yd_think.png"},
        {"[yd_bye]",   ":/res/emoji/yd_bye.png"}
    };

    for (auto it = emojiMap.begin(); it != emojiMap.end(); ++it) {
        const QString imgHtml = QString(
                                    "<img src=\"%1\" width=\"22\" height=\"22\" style=\"vertical-align:middle;\" />"
                                    ).arg(it.value());

        escapedText.replace(it.key().toHtmlEscaped(), imgHtml);
    }

    return escapedText;
}

QString TextBubble::convertToHtml(const QString &text) const
{
    QString html = text.toHtmlEscaped();
    html.replace("\n", "<br/>");
    html = replaceEmojiTokens(html);

    return QString(
               "<html><body style=\"margin:0px; padding:0px; "
               "font-family:'Microsoft YaHei'; font-size:12pt; line-height:1.0;\">%1</body></html>"
               ).arg(html);
}

void TextBubble::setMessageText(const QString &text)
{
    m_pTextEdit->setHtml(convertToHtml(text));
    adjustBubbleSize();
}

void TextBubble::adjustBubbleSize()
{
    QTextDocument *doc = m_pTextEdit->document();
    if (!doc) {
        return;
    }

    doc->setTextWidth(-1);
    qreal ideal = doc->idealWidth();

    int textWidth = qCeil(ideal) + 6;
    if (textWidth < 18) {
        textWidth = 18;
    }
    if (textWidth > maxTextWidth()) {
        textWidth = maxTextWidth();
    }

    doc->setTextWidth(textWidth);

    QSizeF docSize = doc->size();
    int contentW = qCeil(docSize.width());
    int contentH = qCeil(docSize.height());

    m_pTextEdit->setFixedSize(contentW, contentH);

    const QMargins margins = layout()->contentsMargins();
    setFixedSize(m_pTextEdit->width() + margins.left() + margins.right(),
                 m_pTextEdit->height() + margins.top() + margins.bottom());

    updateGeometry();
}

void TextBubble::initStyleSheet()
{
    m_pTextEdit->setStyleSheet(
        "QTextBrowser{"
        "background:transparent;"
        "border:none;"
        "padding:0px;"
        "margin:0px;"
        "}"
        );
}

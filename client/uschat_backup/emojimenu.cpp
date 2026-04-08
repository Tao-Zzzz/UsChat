#include "EmojiMenu.h"

#include <QGridLayout>
#include <QPushButton>
#include <QEvent>
#include <QFocusEvent>
#include <QIcon>

EmojiMenu::EmojiMenu(QWidget *parent)
    : QFrame(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setFocusPolicy(Qt::StrongFocus);

    _emoji_items = {
        {"[yd_smile]", ":/res/emoji/yd_smile.png", "微笑"},
        {"[yd_happy]", ":/res/emoji/yd_happy.png", "开心"},
        {"[yd_love]",  ":/res/emoji/yd_love.png",  "喜欢"},
        {"[yd_shy]",   ":/res/emoji/yd_shy.png",   "害羞"},
        {"[yd_cry]",   ":/res/emoji/yd_cry.png",   "大哭"},
        {"[yd_angry]", ":/res/emoji/yd_angry.png", "生气"},
        {"[yd_sleep]", ":/res/emoji/yd_sleep.png", "困"},
        {"[yd_ok]",    ":/res/emoji/yd_ok.png",    "OK"},
        {"[yd_wow]",   ":/res/emoji/yd_wow.png",   "震惊"},
        {"[yd_sweat]", ":/res/emoji/yd_sweat.png", "流汗"},
        {"[yd_think]", ":/res/emoji/yd_think.png", "思考"},
        {"[yd_bye]",   ":/res/emoji/yd_bye.png",   "拜拜"}
    };

    initUi();

    setStyleSheet(
        "QFrame{"
        "background:#ffffff;"
        "border:1px solid #dcdcdc;"
        "border-radius:10px;"
        "}"
        "QPushButton{"
        "border:none;"
        "background:transparent;"
        "border-radius:6px;"
        "}"
        "QPushButton:hover{"
        "background:#f5f5f5;"
        "}"
        "QPushButton:pressed{"
        "background:#ebebeb;"
        "}"
        );
}

void EmojiMenu::initUi()
{
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    const int columnCount = 4;
    for (int i = 0; i < _emoji_items.size(); ++i) {
        addEmojiButton(_emoji_items[i], i / columnCount, i % columnCount);
    }
}

void EmojiMenu::addEmojiButton(const EmojiItem &item, int row, int col)
{
    auto *btn = new QPushButton(this);
    btn->setFixedSize(42, 42);
    btn->setIcon(QIcon(item.iconPath));
    btn->setIconSize(QSize(28, 28));
    btn->setToolTip(item.tip);

    connect(btn, &QPushButton::clicked, this, [this, item]() {
        emit sig_emoji_selected(item.token);
        hide();
    });

    auto *grid = qobject_cast<QGridLayout*>(layout());
    if (grid) {
        grid->addWidget(btn, row, col);
    }
}

void EmojiMenu::focusOutEvent(QFocusEvent *event)
{
    QFrame::focusOutEvent(event);
    hide();
}

bool EmojiMenu::eventFilter(QObject *watched, QEvent *event)
{
    return QFrame::eventFilter(watched, event);
}

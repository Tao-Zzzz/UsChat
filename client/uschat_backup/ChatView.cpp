#include "ChatView.h"
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QDebug>
#include <QTimer>
#include <QStyleOption>
#include <QPainter>
#include <QMetaObject>

ChatView::ChatView(QWidget *parent)
    : QWidget(parent)
    , isAppended(false)
    , m_batchUpdating(false)
{
    QVBoxLayout *pMainLayout = new QVBoxLayout();
    this->setLayout(pMainLayout);
    pMainLayout->setContentsMargins(0, 0, 0, 0);

    m_pScrollArea = new QScrollArea();
    m_pScrollArea->setObjectName("chat_area");
    pMainLayout->addWidget(m_pScrollArea);

    QWidget *w = new QWidget(this);
    w->setObjectName("chat_bg");
    w->setAutoFillBackground(true);

    m_pVl = new QVBoxLayout();
    m_pVl->setContentsMargins(0, 0, 0, 0);
    m_pVl->setSpacing(0);
    m_pVl->addWidget(new QWidget(), 100000); // 底部撑开项
    w->setLayout(m_pVl);

    m_pScrollArea->setWidget(w);

    m_pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScrollBar *pVScrollBar = m_pScrollArea->verticalScrollBar();
    connect(pVScrollBar, &QScrollBar::rangeChanged, this, &ChatView::onVScrollBarMoved);

    QHBoxLayout *pHLayout_2 = new QHBoxLayout();
    pHLayout_2->addWidget(pVScrollBar, 0, Qt::AlignRight);
    pHLayout_2->setContentsMargins(0, 0, 0, 0);
    m_pScrollArea->setLayout(pHLayout_2);
    pVScrollBar->setHidden(true);

    m_pScrollArea->setWidgetResizable(true);
    m_pScrollArea->installEventFilter(this);
    initStyleSheet();
}

void ChatView::beginBatchUpdate()
{
    m_batchUpdating = true;
    isAppended = false;

    this->setUpdatesEnabled(false);
    m_pScrollArea->setUpdatesEnabled(false);

    if (m_pScrollArea->widget()) {
        m_pScrollArea->widget()->setUpdatesEnabled(false);
    }

    if (m_pVl) {
        m_pVl->setEnabled(false);
    }

    if (m_pScrollArea && m_pScrollArea->verticalScrollBar()) {
        m_pScrollArea->verticalScrollBar()->blockSignals(true);
    }
}

void ChatView::endBatchUpdate(bool scrollToBottomFlag)
{
    QWidget *container = m_pScrollArea ? m_pScrollArea->widget() : nullptr;

    if (container && m_pVl) {
        // 批量阶段插入的 item 统一显示，避免逐条创建时闪成顶层小窗
        for (int i = 0; i < m_pVl->count() - 1; ++i) {
            QLayoutItem *layoutItem = m_pVl->itemAt(i);
            if (!layoutItem) {
                continue;
            }

            QWidget *w = layoutItem->widget();
            if (!w) {
                continue;
            }

            if (w->parentWidget() != container) {
                w->setParent(container);
            }

            w->setWindowFlag(Qt::Window, false);
            w->show();
        }
    }

    if (m_pScrollArea && m_pScrollArea->verticalScrollBar()) {
        m_pScrollArea->verticalScrollBar()->blockSignals(false);
    }

    if (m_pVl) {
        m_pVl->setEnabled(true);
        m_pVl->invalidate();
        m_pVl->activate();
    }

    if (container) {
        container->setUpdatesEnabled(true);
        container->updateGeometry();
        container->adjustSize();
        container->update();
    }

    m_pScrollArea->setUpdatesEnabled(true);
    this->setUpdatesEnabled(true);
    this->updateGeometry();

    m_batchUpdating = false;

    if (scrollToBottomFlag) {
        QMetaObject::invokeMethod(this, [this]() {
            scrollToBottom();
        }, Qt::QueuedConnection);

        QTimer::singleShot(0, this, [this]() {
            scrollToBottom();
        });

        QTimer::singleShot(16, this, [this]() {
            scrollToBottom();
        });
    }

    this->update();
    m_pScrollArea->viewport()->update();
}

void ChatView::scrollToBottom()
{
    if (!m_pScrollArea) {
        return;
    }

    if (m_pVl) {
        m_pVl->invalidate();
        m_pVl->activate();
    }

    if (m_pScrollArea->widget()) {
        m_pScrollArea->widget()->updateGeometry();
    }

    QScrollBar *pVScrollBar = m_pScrollArea->verticalScrollBar();
    if (pVScrollBar) {
        pVScrollBar->setValue(pVScrollBar->maximum());
    }
}

void ChatView::appendChatItem(QWidget *item)
{
    if (!m_pVl || !m_pScrollArea || !m_pScrollArea->widget() || !item) {
        return;
    }

    QWidget *container = m_pScrollArea->widget();

    // 先彻底保证 parent 正确，避免 Qt 把它当顶层窗体闪出来
    if (item->parentWidget() != container) {
        item->hide();
        item->setParent(container);
    }

    // 双保险：明确告诉 Qt 这不是独立窗口
    item->setWindowFlag(Qt::Window, false);

    // 批量模式先隐藏，等 endBatchUpdate 再统一 show
    if (m_batchUpdating) {
        item->hide();
    } else {
        item->show();
    }

    m_pVl->insertWidget(m_pVl->count() - 1, item);

    if (!m_batchUpdating) {
        isAppended = true;
    }
}

void ChatView::prependChatItem(QWidget *item)
{
    Q_UNUSED(item);
}

void ChatView::insertChatItem(QWidget *before, QWidget *item)
{
    Q_UNUSED(before);
    Q_UNUSED(item);
}

void ChatView::removeAllItem()
{
    if (!m_pVl) {
        return;
    }

    while (m_pVl->count() > 1) { // 保留最后一个占位 widget
        QLayoutItem *item = m_pVl->takeAt(0);
        if (!item) {
            continue;
        }

        if (QWidget *widget = item->widget()) {
            delete widget;
        }
        delete item;
    }
}

void ChatView::clearLayoutItemsOnly()
{
    if (!m_pVl) {
        return;
    }

    while (m_pVl->count() > 1) { // 保留最后一个占位 widget
        QLayoutItem *item = m_pVl->takeAt(0);
        if (!item) {
            continue;
        }

        if (QWidget *widget = item->widget()) {
            widget->hide();
        }

        delete item;
    }
}

QList<QWidget*> ChatView::takeAllItems()
{
    QList<QWidget*> items;

    if (!m_pVl) {
        return items;
    }

    while (m_pVl->count() > 1) { // 保留最后一个占位 widget
        QLayoutItem *item = m_pVl->takeAt(0);
        if (!item) {
            continue;
        }

        QWidget *widget = item->widget();
        if (widget) {
            widget->hide();
            items.append(widget);
        }

        delete item;
    }

    return items;
}

void ChatView::setChatItems(const QList<QWidget*>& items)
{
    clearLayoutItemsOnly();

    if (!m_pVl || !m_pScrollArea || !m_pScrollArea->widget()) {
        return;
    }

    QWidget *container = m_pScrollArea->widget();

    for (QWidget *item : items) {
        if (!item) {
            continue;
        }

        if (item->parentWidget() != container) {
            item->hide();
            item->setParent(container);
        }

        item->setWindowFlag(Qt::Window, false);

        if (m_batchUpdating) {
            item->hide();
        } else {
            item->show();
        }

        m_pVl->insertWidget(m_pVl->count() - 1, item);
    }

    m_pVl->invalidate();
    m_pVl->activate();
}

bool ChatView::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Enter && o == m_pScrollArea)
    {
        m_pScrollArea->verticalScrollBar()->setHidden(
            m_pScrollArea->verticalScrollBar()->maximum() == 0);
    }
    else if (e->type() == QEvent::Leave && o == m_pScrollArea)
    {
        m_pScrollArea->verticalScrollBar()->setHidden(true);
    }
    return QWidget::eventFilter(o, e);
}

void ChatView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ChatView::onVScrollBarMoved(int min, int max)
{
    Q_UNUSED(min);
    Q_UNUSED(max);

    if (m_batchUpdating) {
        return;
    }

    if (isAppended)
    {
        scrollToBottom();

        QTimer::singleShot(0, this, [this]()
                           {
                               isAppended = false;
                           });
    }
}

void ChatView::initStyleSheet()
{
}

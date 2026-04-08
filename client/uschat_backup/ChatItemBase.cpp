#include "ChatItemBase.h"
#include <QFont>
#include <QVBoxLayout>
#include "BubbleFrame.h"
ChatItemBase::ChatItemBase(ChatRole role, QWidget *parent)
    : QWidget(parent)
    , m_role(role)
{
    m_pNameLabel    = new QLabel();
    m_pNameLabel->setObjectName("chat_user_name");
    QFont font("Microsoft YaHei");
    font.setPointSize(9);
    m_pNameLabel->setFont(font);
    m_pNameLabel->setFixedHeight(20);
    m_pIconLabel    = new QLabel();
    m_pIconLabel->setScaledContents(true);
    m_pIconLabel->setFixedSize(42, 42);
    m_pBubble       = new QWidget();
    QGridLayout *pGLayout = new QGridLayout();
    pGLayout->setVerticalSpacing(3);
    pGLayout->setHorizontalSpacing(3);
    pGLayout->setContentsMargins(3, 3, 3, 3);
    QSpacerItem*pSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    //添加状态图标控件
    m_pStatusLabel = new QLabel();
    m_pStatusLabel->setFixedSize(16,16);
    m_pStatusLabel->setScaledContents(true);

    if(m_role == ChatRole::Self)
    {
        m_pNameLabel->setContentsMargins(0,0,8,0);
        m_pNameLabel->setAlignment(Qt::AlignRight);

        // 名字在 0行 2列
        pGLayout->addWidget(m_pNameLabel, 0, 2, 1, 1);

        // 头像跨 2行 放在第 3列
        pGLayout->addWidget(m_pIconLabel, 0, 3, 2, 1, Qt::AlignTop);

        // 弹簧在第 0列，给它设置伸缩系数 1
        pGLayout->addItem(pSpacer, 1, 0, 1, 1);

        // 状态图标在第 1列
        pGLayout->addWidget(m_pStatusLabel, 1, 1, 1, 1, Qt::AlignCenter);

        // 【关键修改点】：给气泡增加 Qt::AlignRight 参数
        // 这样即便格子的宽度比气泡宽，气泡也会紧贴右侧（头像方向），而不会向左拉伸
        pGLayout->addWidget(m_pBubble, 1, 2, 1, 1, Qt::AlignRight);

        // 重新校准比例：
        pGLayout->setColumnStretch(0, 1); // 让左边空白区域吃掉所有剩余空间
        pGLayout->setColumnStretch(1, 0); // 固定
        pGLayout->setColumnStretch(2, 0); // 气泡列：不准自动拉伸
        pGLayout->setColumnStretch(3, 0); // 头像列：固定

    }else{
        m_pNameLabel->setContentsMargins(8,0,0,0);
        m_pNameLabel->setAlignment(Qt::AlignLeft);
        pGLayout->addWidget(m_pIconLabel, 0, 0, 2,1, Qt::AlignTop);
        pGLayout->addWidget(m_pNameLabel, 0,1, 1,1);
        pGLayout->addWidget(m_pBubble, 1,1, 1,1);
        pGLayout->addItem(pSpacer, 2, 2, 1, 1);

        // pGLayout->setColumnStretch(1, 3);
        // pGLayout->setColumnStretch(2, 2);
        pGLayout->setColumnStretch(0, 0); // 头像固定
        pGLayout->setColumnStretch(1, 0); // 气泡和名字按需分配
        pGLayout->setColumnStretch(2, 1); // 让右边的弹簧（pSpacer）占据所有剩余空间
    }
    this->setLayout(pGLayout);
}

void ChatItemBase::setUserName(const QString &name)
{
    m_pNameLabel->setText(name);
}

void ChatItemBase::setUserIcon(const QPixmap &icon)
{
    m_pIconLabel->setPixmap(icon);
}

void ChatItemBase::setWidget(QWidget *w)
{
   QGridLayout *pGLayout = (qobject_cast<QGridLayout *>)(this->layout());
   pGLayout->replaceWidget(m_pBubble, w);
   delete m_pBubble;
   m_pBubble = w;
}

void ChatItemBase::setStatus(int status)
{
    if(status == MsgStatus::UN_READ){
        m_pStatusLabel->setPixmap(QPixmap(":/res/unread.png"));
        return ;
    }

    if(status == MsgStatus::SEND_FAILED){
        m_pStatusLabel->setPixmap(QPixmap(":/res/send_fail.png"));
        return ;
    }

    if(status == MsgStatus::READED){
        m_pStatusLabel->setPixmap(QPixmap(":/res/readed.png"));
        return ;
    }
}


QLabel* ChatItemBase::getIconLabel() {
    return m_pIconLabel;
}

QWidget* ChatItemBase::getBubble() {
    return m_pBubble;
}

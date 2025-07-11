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
    m_pStatusLabel = new QLabel();// 加到布局里,布局会成为它的父亲
    m_pStatusLabel->setFixedSize(16, 16);
    m_pStatusLabel->setScaledContents(true);

    if(m_role == ChatRole::Self)
    {
        // 间隔右边8格
        m_pNameLabel->setContentsMargins(0,0,8,0);
        m_pNameLabel->setAlignment(Qt::AlignRight);
        // pGLayout->addWidget(m_pNameLabel, 0,1, 1,1);//0行1列占1行1列
        // pGLayout->addWidget(m_pIconLabel, 0, 2, 2,1, Qt::AlignTop);
        // pGLayout->addItem(pSpacer, 1, 0, 1, 1);
        // pGLayout->addWidget(m_pBubble, 1,1, 1,1, Qt::AlignRight);

        pGLayout->addWidget(m_pNameLabel, 0,2, 1,1);//0行1列占1行1列
        pGLayout->addWidget(m_pIconLabel, 0, 3, 2,1, Qt::AlignTop);
        pGLayout->addItem(pSpacer, 1, 0, 1, 1);
        pGLayout->addWidget(m_pStatusLabel, 1, 1, 1, 1, Qt::AlignCenter);
        pGLayout->addWidget(m_pBubble, 1,2, 1,1, Qt::AlignRight);// bubule到第一行第2列去了

        // 伸缩所占比例, 0列占2, 1列占3
        pGLayout->setColumnStretch(0, 2); // 比例占2
        pGLayout->setColumnStretch(1, 0); // Status图标固定大小
        // pGLayout->setColumnStretch(1, 3);
        pGLayout->setColumnStretch(2, 3); // bubule的拉伸, 第2列
        pGLayout->setColumnStretch(3, 0);
        // 在上面设置固定了大小的组件是不会拉伸的
    }else{
        m_pNameLabel->setContentsMargins(8,0,0,0);
        m_pNameLabel->setAlignment(Qt::AlignLeft);
        pGLayout->addWidget(m_pIconLabel, 0, 0, 2,1, Qt::AlignTop);
        pGLayout->addWidget(m_pNameLabel, 0,1, 1,1);
        pGLayout->addWidget(m_pBubble, 1,1, 1,1, Qt::AlignLeft);
        pGLayout->addItem(pSpacer, 2, 2, 1, 1);
        pGLayout->setColumnStretch(1, 3);
        pGLayout->setColumnStretch(2, 2);
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
    // 先占位后替换
    pGLayout->replaceWidget(m_pBubble, w);
   // 手动释放,没有人会释放他了以后
   delete m_pBubble;
   m_pBubble = w;
}

void ChatItemBase::setState(int status)
{
    if(status == MsgStatus::UN_READ){
        m_pStatusLabel->setPixmap(QPixmap(":/res/unread.png"));
        return;
    }
    if(status == MsgStatus::SEND_FAILED){
        m_pStatusLabel->setPixmap(QPixmap(":/res/send_fail.png"));
        return;
    }
    if(status == MsgStatus::READED){
        m_pStatusLabel->setPixmap(QPixmap(":/res/readed.png"));
        return;
    }
}

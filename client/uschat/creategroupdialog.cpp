#include "creategroupdialog.h"

CreateGroupDialog::CreateGroupDialog(QWidget *parent) : QDialog(parent) {
    initUI();
    loadFriendList();
    setWindowTitle("创建群聊");
    setFixedSize(650, 550);
}

void CreateGroupDialog::initUI() {
    // 整体 QSS 样式
    this->setStyleSheet("QDialog { background-color: #262626; }"
                        "QListWidget { background-color: transparent; border: none; outline: none; }"
                        "QLineEdit { background-color: #333333; border: 1px solid #444; border-radius: 4px; color: white; padding: 5px; }");

    auto* mainLayout = new QHBoxLayout(this);

    // --- 左半部分布局 ---
    auto* leftLayout = new QVBoxLayout();
    _searchEdit = new QLineEdit(this);
    _searchEdit->setPlaceholderText("搜索");
    _friendListWidget = new QListWidget(this);
    _friendListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    leftLayout->addWidget(new QLabel("选择好友创建", this));
    leftLayout->addWidget(_searchEdit);
    leftLayout->addWidget(_friendListWidget);

    // --- 右半部分布局 ---
    auto* rightLayout = new QVBoxLayout();
    _countLabel = new QLabel("已选 0 个联系人", this);
    _countLabel->setStyleSheet("color: gray;");
    _selectedListWidget = new QListWidget(this);

    auto* btnLayout = new QHBoxLayout();
    auto* okBtn = new QPushButton("确定", this);
    okBtn->setObjectName("okBtn");
    okBtn->setStyleSheet("QPushButton#okBtn { background-color: #007bff; color: white; border-radius: 4px; padding: 8px 20px; }");
    auto* cancelBtn = new QPushButton("取消", this);
    cancelBtn->setStyleSheet("background-color: #444; color: white; border-radius: 4px; padding: 8px 20px;");

    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);

    rightLayout->addWidget(_countLabel);
    rightLayout->addWidget(_selectedListWidget);
    rightLayout->addLayout(btnLayout);

    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addSpacing(10);
    mainLayout->addLayout(rightLayout, 1);

    // 信号连接
    connect(_searchEdit, &QLineEdit::textChanged, this, &CreateGroupDialog::onSearchTextChanged);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void CreateGroupDialog::loadFriendList() {
    // 假设从 UserMgr 获取
    auto friends = UserMgr::GetInstance()->GetFriendList();

    // 1. 排序 (按昵称升序)
    std::sort(friends.begin(), friends.end(), [](const std::shared_ptr<UserInfo>& a, const std::shared_ptr<UserInfo>& b){
        return a->_nick < b->_nick;
    });

    // 2. 填充左侧列表
    for(auto& info : friends) {
        QListWidgetItem* item = new QListWidgetItem(_friendListWidget);
        item->setSizeHint(QSize(0, 50));
        FriendCheckItem* widget = new FriendCheckItem(info, this);
        _friendListWidget->setItemWidget(item, widget);
        _left_widgets[info->_uid] = widget;

        connect(widget, &FriendCheckItem::sig_checkClicked, this, &CreateGroupDialog::onFriendChecked);
    }
}

void CreateGroupDialog::onFriendChecked(std::shared_ptr<UserInfo> info, bool checked) {
    if(checked) {
        // 右侧添加
        QListWidgetItem* item = new QListWidgetItem(_selectedListWidget);
        item->setSizeHint(QSize(0, 45));
        SelectedFriendItem* widget = new SelectedFriendItem(info, this);
        _selectedListWidget->setItemWidget(item, widget);

        _right_items[info->_uid] = item;
        connect(widget, &SelectedFriendItem::sig_removeClicked, this, &CreateGroupDialog::onRemoveSelected);
    } else {
        // 右侧移除
        onRemoveSelected(info->_uid);
    }
    updateSelectedCount();
}

void CreateGroupDialog::onRemoveSelected(int uid) {
    // 1. 移除右侧视觉项
    if(_right_items.count(uid)) {
        int row = _selectedListWidget->row(_right_items[uid]);
        _selectedListWidget->takeItem(row);
        _right_items.erase(uid);
    }

    // 2. 同步左侧 Checkbox 状态
    if(_left_widgets.count(uid)) {
        _left_widgets[uid]->setCheckState(false);
    }
    updateSelectedCount();
}

void CreateGroupDialog::onSearchTextChanged(const QString& text) {
    for(int i = 0; i < _friendListWidget->count(); ++i) {
        auto* item = _friendListWidget->item(i);
        auto* widget = dynamic_cast<FriendCheckItem*>(_friendListWidget->itemWidget(item));
        if(widget) {
            bool match = widget->getInfo()->_nick.contains(text, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    }
}

void CreateGroupDialog::updateSelectedCount() {
    _countLabel->setText(QString("已选 %1 个联系人").arg(_right_items.size()));
}

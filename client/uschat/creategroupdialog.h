#ifndef CREATEGROUPDIALOG_H
#define CREATEGROUPDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <memory>
#include <vector>
#include <map>
#include <algorithm>
#include "usermgr.h"


// --- 1. 左侧：带复选框的好友项 ---
class FriendCheckItem : public QWidget {
    Q_OBJECT
public:
    FriendCheckItem(std::shared_ptr<UserInfo> info, QWidget* parent = nullptr)
        : QWidget(parent), _info(info) {
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 5, 10, 5);

        _checkBox = new QCheckBox(this);
        _avatar = new QLabel(this);
        _avatar->setFixedSize(40, 40);
        _avatar->setScaledContents(true);
        _avatar->setPixmap(QPixmap(info->_icon)); // 实际使用请处理资源路径

        _nick = new QLabel(info->_nick, this);
        _nick->setStyleSheet("color: white; font-size: 14px;");

        layout->addWidget(_checkBox);
        layout->addWidget(_avatar);
        layout->addWidget(_nick);
        layout->addStretch();

        connect(_checkBox, &QCheckBox::stateChanged, [this](int state){
            emit sig_checkClicked(_info, state == Qt::Checked);
        });
    }

    void setCheckState(bool checked) {
        QSignalBlocker blocker(_checkBox); // 防止触发信号导致死循环
        _checkBox->setChecked(checked);
    }

    std::shared_ptr<UserInfo> getInfo() const { return _info; }

signals:
    void sig_checkClicked(std::shared_ptr<UserInfo> info, bool checked);

private:
    QCheckBox* _checkBox;
    QLabel* _avatar;
    QLabel* _nick;
    std::shared_ptr<UserInfo> _info;
};

// --- 2. 右侧：已选择的好友项（带删除按钮） ---
class SelectedFriendItem : public QWidget {
    Q_OBJECT
public:
    SelectedFriendItem(std::shared_ptr<UserInfo> info, QWidget* parent = nullptr)
        : QWidget(parent), _info(info) {
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(5, 5, 5, 5);

        _avatar = new QLabel(this);
        _avatar->setFixedSize(35, 35);
        _avatar->setScaledContents(true);
        _avatar->setPixmap(QPixmap(info->_icon));

        _nick = new QLabel(info->_nick, this);
        _nick->setStyleSheet("color: #bbbbbb;");

        _delBtn = new QPushButton("×", this);
        _delBtn->setFixedSize(20, 20);
        _delBtn->setStyleSheet("QPushButton{ background:transparent; color:gray; font-size:18px; border:none; }"
                               "QPushButton:hover{ color:red; }");

        layout->addWidget(_avatar);
        layout->addWidget(_nick);
        layout->addStretch();
        layout->addWidget(_delBtn);

        connect(_delBtn, &QPushButton::clicked, [this](){
            emit sig_removeClicked(_info->_uid);
        });
    }

signals:
    void sig_removeClicked(int uid);

private:
    QLabel* _avatar;
    QLabel* _nick;
    QPushButton* _delBtn;
    std::shared_ptr<UserInfo> _info;
};

// CreateGroupDialog 类定义
class CreateGroupDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateGroupDialog(QWidget *parent = nullptr);

private slots:
    void onSearchTextChanged(const QString& text);
    void onFriendChecked(std::shared_ptr<UserInfo> info, bool checked);
    void onRemoveSelected(int uid);

private:
    void initUI();
    void loadFriendList();
    void updateSelectedCount();

    QLineEdit* _searchEdit;
    QListWidget* _friendListWidget;   // 左侧列表
    QListWidget* _selectedListWidget; // 右侧列表
    QLabel* _countLabel;              // "已选 X 个联系人"

    // 快速查找映射：UID -> 左侧 Item 的 Widget
    std::map<int, FriendCheckItem*> _left_widgets;
    // 快速查找映射：UID -> 右侧 ListWidgetItem (用于删除)
    std::map<int, QListWidgetItem*> _right_items;
};
#endif // CREATEGROUPDIALOG_H

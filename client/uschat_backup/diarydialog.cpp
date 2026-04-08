#include "diarydialog.h"

#include <QTableView>
#include <QPushButton>
#include <QLineEdit>
#include <QDateTimeEdit>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QStandardPaths>

DiaryDialog::DiaryDialog(QWidget* parent)
    : QDialog(parent),
    _tableView(nullptr),
    _btnAdd(nullptr),
    _btnRemove(nullptr),
    _btnFinish(nullptr),
    _btnClose(nullptr),
    _editThing(nullptr),
    _editStart(nullptr),
    _editEnd(nullptr),
    _model(new DiaryTableModel(this))
{
    setWindowTitle(QStringLiteral("日志清单"));
    setModal(true);
    resize(900, 600);

    InitUi();
    InitConnect();
    LoadFromJson();
}

void DiaryDialog::InitUi()
{
    _tableView = new QTableView(this);
    _tableView->setModel(_model);
    _tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    _tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    _tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _tableView->setAlternatingRowColors(true);
    _tableView->horizontalHeader()->setStretchLastSection(true);
    _tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    _tableView->verticalHeader()->setVisible(false);
    _tableView->setColumnWidth(DiaryTableModel::ColThing, 260);
    _tableView->setColumnWidth(DiaryTableModel::ColStartTime, 180);
    _tableView->setColumnWidth(DiaryTableModel::ColEndTime, 180);
    _tableView->setColumnWidth(DiaryTableModel::ColFinished, 100);

    QLabel* lbTitle = new QLabel(QStringLiteral("新增事项"), this);

    _editThing = new QLineEdit(this);
    _editThing->setPlaceholderText(QStringLiteral("请输入要做的事情"));

    _editStart = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    _editStart->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    _editStart->setCalendarPopup(true);

    _editEnd = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(3600), this);
    _editEnd->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    _editEnd->setCalendarPopup(true);

    _btnAdd = new QPushButton(QStringLiteral("新增"), this);
    _btnRemove = new QPushButton(QStringLiteral("删除"), this);
    _btnFinish = new QPushButton(QStringLiteral("标记完成"), this);
    _btnClose = new QPushButton(QStringLiteral("关闭"), this);

    _btnRemove->setEnabled(false);
    _btnFinish->setEnabled(false);

    QFormLayout* formLayout = new QFormLayout;
    formLayout->addRow(QStringLiteral("什么事："), _editThing);
    formLayout->addRow(QStringLiteral("发起时间："), _editStart);
    formLayout->addRow(QStringLiteral("结束时间："), _editEnd);

    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addWidget(_btnAdd);
    btnLayout->addWidget(_btnRemove);
    btnLayout->addWidget(_btnFinish);
    btnLayout->addStretch();
    btnLayout->addWidget(_btnClose);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(lbTitle);
    mainLayout->addLayout(formLayout);
    mainLayout->addSpacing(8);
    mainLayout->addLayout(btnLayout);
    mainLayout->addSpacing(12);
    mainLayout->addWidget(_tableView);

    setLayout(mainLayout);
}

void DiaryDialog::InitConnect()
{
    connect(_btnAdd, &QPushButton::clicked, this, &DiaryDialog::slot_add_task);
    connect(_btnRemove, &QPushButton::clicked, this, &DiaryDialog::slot_remove_task);
    connect(_btnFinish, &QPushButton::clicked, this, &DiaryDialog::slot_mark_finished);
    connect(_btnClose, &QPushButton::clicked, this, &DiaryDialog::accept);

    connect(_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &DiaryDialog::slot_selection_changed);

    // 关键：model 数据变化时自动保存
    connect(_model, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex&, const QModelIndex&, const QList<int>&) {
                SaveToJson();
            });
}

QString DiaryDialog::GetJsonPath() const
{
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir;
    if (!dir.exists(dirPath)) {
        dir.mkpath(dirPath);
    }
    return dirPath + "/diary_tasks.json";
}

void DiaryDialog::LoadFromJson()
{
    QFile file(GetJsonPath());
    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return;
    }

    QJsonArray arr = doc.array();
    QVector<DiaryTask> tasks;
    for (const QJsonValue& val : arr) {
        if (!val.isObject()) {
            continue;
        }
        tasks.push_back(DiaryTask::FromJson(val.toObject()));
    }

    _model->SetTasks(tasks);
}

void DiaryDialog::SaveToJson() const
{
    QJsonArray arr;
    const QVector<DiaryTask>& tasks = _model->GetTasks();
    for (const DiaryTask& task : tasks) {
        arr.append(task.ToJson());
    }

    QJsonDocument doc(arr);

    QFile file(GetJsonPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void DiaryDialog::slot_add_task()
{
    QString thing = _editThing->text().trimmed();
    if (thing.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入要做的事情"));
        return;
    }

    if (_editEnd->dateTime() < _editStart->dateTime()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("结束时间不能早于发起时间"));
        return;
    }

    DiaryTask task;
    task.thing = thing;
    task.startTime = _editStart->dateTime();
    task.endTime = _editEnd->dateTime();
    task.finished = false;

    _model->AddTask(task);
    SaveToJson();

    _editThing->clear();
    _editStart->setDateTime(QDateTime::currentDateTime());
    _editEnd->setDateTime(QDateTime::currentDateTime().addSecs(3600));
}

void DiaryDialog::slot_remove_task()
{
    QModelIndex index = _tableView->currentIndex();
    if (!index.isValid()) {
        return;
    }

    _model->RemoveTask(index.row());
    SaveToJson();
}

void DiaryDialog::slot_mark_finished()
{
    QModelIndex index = _tableView->currentIndex();
    if (!index.isValid()) {
        return;
    }

    QModelIndex finishIndex = _model->index(index.row(), DiaryTableModel::ColFinished);
    _model->setData(finishIndex, Qt::Checked, Qt::CheckStateRole);
    SaveToJson();
}

void DiaryDialog::slot_selection_changed()
{
    bool hasSelection = _tableView->selectionModel()->hasSelection();
    _btnRemove->setEnabled(hasSelection);
    _btnFinish->setEnabled(hasSelection);
}

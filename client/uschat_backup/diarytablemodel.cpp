#include "diarytablemodel.h"

DiaryTableModel::DiaryTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int DiaryTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return _tasks.size();
}

int DiaryTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColCount;
}

QVariant DiaryTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() < 0 || index.row() >= _tasks.size()) {
        return QVariant();
    }

    const DiaryTask& task = _tasks[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColThing:
            return task.thing;
        case ColStartTime:
            return task.startTime.toString("yyyy-MM-dd HH:mm:ss");
        case ColEndTime:
            return task.endTime.toString("yyyy-MM-dd HH:mm:ss");
        case ColFinished:
            return task.finished ? QStringLiteral("已完成") : QStringLiteral("未完成");
        default:
            return QVariant();
        }
    }

    if (role == Qt::CheckStateRole && index.column() == ColFinished) {
        return task.finished ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }

    return QVariant();
}

QVariant DiaryTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case ColThing:
            return QStringLiteral("什么事");
        case ColStartTime:
            return QStringLiteral("发起时间");
        case ColEndTime:
            return QStringLiteral("结束时间");
        case ColFinished:
            return QStringLiteral("是否完成");
        default:
            return QVariant();
        }
    }

    return section + 1;
}

Qt::ItemFlags DiaryTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (index.column() == ColFinished) {
        f |= Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
    }
    return f;
}

bool DiaryTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    if (index.row() < 0 || index.row() >= _tasks.size()) {
        return false;
    }

    DiaryTask& task = _tasks[index.row()];

    if (index.column() == ColFinished && role == Qt::CheckStateRole) {
        task.finished = (value.toInt() == Qt::Checked);
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::CheckStateRole});
        return true;
    }

    return false;
}

void DiaryTableModel::AddTask(const DiaryTask& task)
{
    int row = _tasks.size();
    beginInsertRows(QModelIndex(), row, row);
    _tasks.push_back(task);
    endInsertRows();
}

void DiaryTableModel::RemoveTask(int row)
{
    if (row < 0 || row >= _tasks.size()) {
        return;
    }

    beginRemoveRows(QModelIndex(), row, row);
    _tasks.removeAt(row);
    endRemoveRows();
}

void DiaryTableModel::UpdateTask(int row, const DiaryTask& task)
{
    if (row < 0 || row >= _tasks.size()) {
        return;
    }

    _tasks[row] = task;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

const DiaryTask* DiaryTableModel::GetTask(int row) const
{
    if (row < 0 || row >= _tasks.size()) {
        return nullptr;
    }
    return &_tasks[row];
}

DiaryTask* DiaryTableModel::GetTask(int row)
{
    if (row < 0 || row >= _tasks.size()) {
        return nullptr;
    }
    return &_tasks[row];
}

void DiaryTableModel::SetTasks(const QVector<DiaryTask>& tasks)
{
    beginResetModel();
    _tasks = tasks;
    endResetModel();
}

const QVector<DiaryTask>& DiaryTableModel::GetTasks() const
{
    return _tasks;
}

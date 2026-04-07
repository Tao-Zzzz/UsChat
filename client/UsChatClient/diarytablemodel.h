#ifndef DIARYTABLEMODEL_H
#define DIARYTABLEMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include "diarytask.h"

class DiaryTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit DiaryTableModel(QObject* parent = nullptr);

    enum DiaryColumn
    {
        ColThing = 0,
        ColStartTime,
        ColEndTime,
        ColFinished,
        ColCount
    };

public:
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

public:
    void AddTask(const DiaryTask& task);
    void RemoveTask(int row);
    void UpdateTask(int row, const DiaryTask& task);

    const DiaryTask* GetTask(int row) const;
    DiaryTask* GetTask(int row);

    void SetTasks(const QVector<DiaryTask>& tasks);
    const QVector<DiaryTask>& GetTasks() const;

private:
    QVector<DiaryTask> _tasks;
};

#endif // DIARYTABLEMODEL_H

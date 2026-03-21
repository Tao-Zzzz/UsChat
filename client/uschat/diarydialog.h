#ifndef DIARYDIALOG_H
#define DIARYDIALOG_H

#include <QDialog>

class QTableView;
class QPushButton;
class QLineEdit;
class QDateTimeEdit;

#include "diarytablemodel.h"

class DiaryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DiaryDialog(QWidget* parent = nullptr);

private slots:
    void slot_add_task();
    void slot_remove_task();
    void slot_mark_finished();
    void slot_selection_changed();

private:
    void InitUi();
    void InitConnect();
    void LoadFromJson();
    void SaveToJson() const;
    QString GetJsonPath() const;

private:
    QTableView* _tableView;
    QPushButton* _btnAdd;
    QPushButton* _btnRemove;
    QPushButton* _btnFinish;
    QPushButton* _btnClose;

    QLineEdit* _editThing;
    QDateTimeEdit* _editStart;
    QDateTimeEdit* _editEnd;

    DiaryTableModel* _model;
};

#endif // DIARYDIALOG_H

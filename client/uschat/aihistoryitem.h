#ifndef AIHISTORYITEM_H
#define AIHISTORYITEM_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>

class AiHistoryItem : public QWidget {
    Q_OBJECT
public:
    explicit AiHistoryItem(const QString& name,
                           const QString& date,
                           QWidget* parent = nullptr);

    QString historyName() const { return _name; }

signals:
    void sig_clicked(const QString& name);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    QString _name;
};

#endif // AIHISTORYITEM_H

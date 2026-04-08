#ifndef AIMODELITEM_H
#define AIMODELITEM_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>

class AiModelItem : public QWidget
{
    Q_OBJECT
public:
    explicit AiModelItem(const QString& name,
                         const QPixmap& icon,
                         QWidget* parent = nullptr);

signals:
    void sig_clicked(const QString& modelName);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    QString _modelName;
};

#endif // AIMODELITEM_H

#include "clickedbtn.h"
#include <QVariant>
#include "global.h"

ClickedBtn::ClickedBtn(QWidget *parent):QPushButton (parent)
{
    setCursor(Qt::PointingHandCursor); // 设置光标为小手
    setFocusPolicy(Qt::NoFocus);
}

ClickedBtn::~ClickedBtn(){

}


void ClickedBtn::SetState(QString normal, QString hover, QString press)
{
    _hover = hover;
    _normal = normal;
    _press = press;
    setProperty("state",normal);
    repolish(this);
    update();
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
// Qt5+ 实现
void ClickedBtn::enterEvent(QEnterEvent *event) {
    setProperty("state",_hover);
    repolish(this);
    update();
    QPushButton::enterEvent(event);
}
#else
// Qt4 实现
void ClickedBtn::enterEvent(QEvent *event)
{
    setProperty("state",_hover);
    repolish(this);
    update();
    QPushButton::enterEvent(event);
}
#endif


void ClickedBtn::leaveEvent(QEvent *event)
{
    setProperty("state",_normal);
    repolish(this);
    update();
    QPushButton::leaveEvent(event);
}

void ClickedBtn::mousePressEvent(QMouseEvent *event)
{
    setProperty("state",_press);
    repolish(this);
    update();
    QPushButton::mousePressEvent(event);
}

void ClickedBtn::mouseReleaseEvent(QMouseEvent *event)
{
    setProperty("state",_hover);
    repolish(this);
    update();
    QPushButton::mouseReleaseEvent(event);
}

#include "cursorwidget.h"
#include <QPainter>

CursorWidget::CursorWidget(QWidget *parent, const QColor& color)
    : QWidget(parent), m_color(color)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setFixedWidth(1);
    setVisible(false);
}

void CursorWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), m_color);
}

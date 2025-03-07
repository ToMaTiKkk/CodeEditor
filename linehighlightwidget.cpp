#include "linehighlightwidget.h"
#include <QPainter>

LineHighlightWidget::LineHighlightWidget(QWidget *parent, const QColor& color)
    : QWidget(parent), m_color(color)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setVisible(false);
}

void LineHighlightWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setOpacity(0.08);
    painter.fillRect(rect(), m_color);
}

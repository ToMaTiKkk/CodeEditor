#include "cursorwidget.h"
#include <QPainter>

CursorWidget::CursorWidget(QWidget *parent, const QColor& color)
    : QWidget(parent), m_color(color)
{
    setAttribute(Qt::WA_TransparentForMouseEvents); // виджет не принимает мышинные события, курсор только для отображения, события мыши не перехватываются
    setAttribute(Qt::WA_NoSystemBackground); // отказываемся от фоновой отрисовки системой, виджет прозрачный, кромер самого курсора
    setAutoFillBackground(false);
    setFixedWidth(1);
    setVisible(false); // по умолчанию курсор не видно

    m_blinkTimer.setInterval(500); // каждые 500 мс переключение видомости курсора
    connect(&m_blinkTimer, &QTimer::timeout, this, [this]() { // при каждом срабатывании таймера срабатывает лямбда-функции, которая переключается видимость
        setVisible(!isVisible());
        update();
    });
    m_blinkTimer.start();
}

void CursorWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), m_color); // заполняем виджет выбраным цветом
}

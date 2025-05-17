#include "cursorwidget.h"
#include <QPainter>
#include <QDebug>

CursorWidget::CursorWidget(QWidget *parent, const QColor& color)
    : QWidget(parent), m_color(color)
{
    //setAttribute(Qt::WA_TransparentForMouseEvents); // виджет не принимает мышинные события, курсор только для отображения, события мыши не перехватываются
    setAttribute(Qt::WA_NoSystemBackground); // отказываемся от фоновой отрисовки системой, виджет прозрачный, кромер самого курсора
    setAutoFillBackground(false);
    setFixedWidth(2);
    setVisible(false); // по умолчанию курсор не видно

    m_blinkTimer.setInterval(500); // каждые 500 мс переключение видомости курсора
    connect(&m_blinkTimer, &QTimer::timeout, this, [this]() { // при каждом срабатывании таймера срабатывает лямбда-функции, которая переключается видимость
        setVisible(!isVisible());
        update();
    });
    m_blinkTimer.start();

    m_customToolTip = new CustomToolTip(parent);
    m_customToolTip->setCustomStyle(color);
    m_customToolTip->hide();
}

CursorWidget::~CursorWidget()
{
    if (m_customToolTip)
    {
        m_customToolTip->hide();
        m_customToolTip->deleteLater();
    }
}

void CursorWidget::paintEvent(QPaintEvent *event)
{
    /*QPainter painter(this);
    painter.fillRect(rect(), m_color);*/ // заполняем виджет выбраным цветом
    QPainter painter(this);
    painter.setPen(QColor(184, 64, 245));
    painter.setOpacity(0.8);
    painter.drawLine(0, 0, 0, height());
    painter.setOpacity(0.3);
    painter.fillRect(rect(), QColor(184, 64, 245));
}

void CursorWidget::setUsername(const QString &username)
{
    m_username = username;
    m_customToolTip->setText(username);
    m_customToolTip->adjustSize();
    m_customToolTip->move(this->pos() + QPoint(11, -9));
    // m_customToolTip->move(mapToGlobal(QPoint(11, -9)));
    m_customToolTip->show();
    qDebug() << "CursorWidget username set to:" << username;
}


void CursorWidget::setCustomToolTipStyle(const QColor &color)
{
    m_customToolTip->setCustomStyle(color);
}


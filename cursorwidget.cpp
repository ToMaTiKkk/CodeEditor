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

    // m_usernameLabel = new QLabel(parent);
    // // m_usernameLabel->setStyleSheet("background-color: rgba(0, 0, 0, 255); color: white; padding: 2px;"); // полностью непрозрачный черный
    // m_usernameLabel->hide();
    m_customToolTip = new CustomToolTip(parent);
    m_customToolTip->setCustomStyle(color);
    m_customToolTip->hide();
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
    /*setToolTip(username);
    m_usernameLabel->setText(username);
    m_usernameLabel->adjustSize(); // автоматически изменяет размер, чтобы поместился весь текст
    m_usernameLabel->show();
    m_usernameLabel->move(this->pos() + QPoint(11, -9)); */// позиционирование метки рядом с курсором, на 2 пикселя вправо и 5 вверх
    m_customToolTip->setText(username);
    m_customToolTip->adjustSize();
    m_customToolTip->move(this->pos() + QPoint(11, -9));
    // m_customToolTip->move(mapToGlobal(QPoint(11, -9)));
    m_customToolTip->show();
    qDebug() << "CursorWidget username set to:" << username;
}

// void CursorWidget::setStyleSheetForToolTip(const QColor& color)
// {
//     QString styleSheet = QString(
//         "QToolTip {"
//         "   background-color: %1;"
//         "   color: white;"
//         "   border: 1px solid %2;"
//         "   padding: 2px 5px;"
//         "   border-radius: 3px;"
//         "   opacity: 200;"
//         "}"
//                              ).arg(color.name()).arg(color.darker().name());
//     setStyleSheet(styleSheet);
// }

void CursorWidget::setCustomToolTipStyle(const QColor &color)
{
    m_customToolTip->setCustomStyle(color);
}

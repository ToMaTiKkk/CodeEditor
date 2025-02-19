#ifndef CURSORWIDGET_H
#define CURSORWIDGET_H

#include <QWidget>
#include <QColor>
#include <QTimer>

class CursorWidget : public QWidget
{
    Q_OBJECT
public:
    // конструктор, принимающий родительский виджет и цвет курсора (по умолчанию красный)
    explicit CursorWidget(QWidget *parent = nullptr, const QColor& color = Qt::red);
    QColor getColor() const { return m_color; } // получение цвета курсора
    void setColor(const QColor &color) { m_color = color, update(); } // изменение цвета курсора

protected:
    // переопределение метода, чтобы отрисовывать курсор
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_color; // цвет курсора
    QTimer m_blinkTimer;
};

#endif

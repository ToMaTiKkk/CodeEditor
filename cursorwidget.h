#ifndef CURSORWIDGET_H
#define CURSORWIDGET_H

#include <QWidget>
#include <QColor>

class CursorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CursorWidget(QWidget *parent = nullptr, const QColor& color = Qt::red);
    QColor getColor() const { return m_color; }
    void setColor(const QColor &color) { m_color = color, update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_color;
};

#endif

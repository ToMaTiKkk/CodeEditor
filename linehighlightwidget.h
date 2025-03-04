#ifndef LINEHIGHLIGHTWIDGET_H
#define LINEHIGHLIGHTWIDGET_H

#include <QWidget>
#include <QColor>

class LineHighlightWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LineHighlightWidget(QWidget *parent = nullptr, const QColor& color = Qt::lightGray);
    void setColor(const QColor &color) { m_color = color; update(); }
    QColor color() const { return m_color; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_color;
};

#endif

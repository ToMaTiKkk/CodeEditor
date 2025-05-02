#ifndef DIAGNOSTICTOOLTIP_H
#define DIAGNOSTICTOOLTIP_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPainter>

class DiagnosticTooltip : public QWidget
{
    Q_OBJECT

public:
    explicit DiagnosticTooltip(QWidget *parent = nullptr);
    void setText(const QString &text);
    void setRichText(const QString &richText); // для форматирования
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel *m_label;
};

#endif

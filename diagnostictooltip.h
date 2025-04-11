#ifndef DIAGNOSTICTOOLTIP_H
#define DIAGNOSTICTOOLTIP_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

class DiagnosticTooltip : public QWidget
{
    Q_OBJECT

public:
    explicit DiagnosticTooltip(QWidget *parent = nullptr);
    void setText(const QString &text);
    void setRichText(const QString &richText); // для форматирования
    QPoint calculateTooltipPosition(const QPoint& globalMousePos);

private:
    QLabel *m_label;
    DiagnosticTooltip* m_diagnosticTooltip;
};

#endif

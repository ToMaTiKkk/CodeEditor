// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

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

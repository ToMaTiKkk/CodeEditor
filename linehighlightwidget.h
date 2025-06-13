// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LINEHIGHLIGHTWIDGET_H
#define LINEHIGHLIGHTWIDGET_H

#include "codeplaintextedit.h"
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

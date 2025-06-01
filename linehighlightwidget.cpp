// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#include "linehighlightwidget.h"
#include <QPainter>

LineHighlightWidget::LineHighlightWidget(QWidget *parent, const QColor& color)
    : QWidget(parent), m_color(color)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setVisible(false);
}

void LineHighlightWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setOpacity(0.08);
    painter.fillRect(rect(), m_color);
}

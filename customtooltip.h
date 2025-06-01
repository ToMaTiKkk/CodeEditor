// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CUSTOMTOOLTIP_H
#define CUSTOMTOOLTIP_H

#include <QLabel>
#include <QWidget>
#include <QColor>
#include <QGraphicsOpacityEffect>

class CustomToolTip : public QLabel {
    Q_OBJECT

public:
    explicit CustomToolTip(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        //setWindowFlag(Qt::ToolTip);
        setAttribute(Qt::WA_ShowWithoutActivating);
    }

    void setCustomStyle(const QColor &color) {
        QString styleSheet = QString(
            "background-color: %1;"
            "color: white;"
            "border: 1px solid %2;"
            "padding: 1px 3px;"
            "border-radius: 5px;"
            "font-size: 9px;"
        ).arg(color.name()).arg(color.darker().name());
        setStyleSheet(styleSheet);

        QGraphicsOpacityEffect *opacityEffect = new QGraphicsOpacityEffect(this);
        opacityEffect->setOpacity(0.75);  // 60% непрозрачности
        setGraphicsEffect(opacityEffect);

    }
};

#endif

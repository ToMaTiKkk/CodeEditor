// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CODEPLAINTEXTEDIT_H
#define CODEPLAINTEXTEDIT_H

#include <QKeyEvent>
#include <QTextCursor>
#include <QPlainTextEdit>

// класс для автоскобок и
class CodePlainTextEdit : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit CodePlainTextEdit(QWidget *parent = nullptr);

    QRectF getBlockBoundingRect(const QTextBlock &block) const; // получение геометрии блока
    QTextBlock getFirstVisibleBlock() const;
    QRectF getBlockBoundingGeometry(const QTextBlock &block) const;
    QPointF getContentOffset() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent* event) override;
    void smoothScrollBy(int deltaY);

signals:
    void completionShortcut(); // при crtl+space
    void definitionnShortcut(); // F12
};

#endif //CODEPLAINTEXTEDIT_H

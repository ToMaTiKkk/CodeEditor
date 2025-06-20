// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VIRTUALCODEEDITOR_H
#define VIRTUALCODEEDITOR_H

#include "documentmodel.h"
#include <QAbstractScrollArea>
#include <QFont>
#include <QTextCursor>

// полностью виртуализированный редактор, занимается только отрисовкой текста
class VirtualCodeEditor : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit VirtualCodeEditor(DocumentModel *model, QWidget *parent = nullptr);

    // методы интеграции с самой IDE
    int getCursorPosition() const;
    void setCursorPosition(int position);
    QTextCursor textCursor() const;
    void setTextCursor(const QTextCursor& cursor);

    // для нумерации строк
    int firstVisibleBlockNumber() const;
    int visibleBlockCount() const;
    QRect getBlockBoundingRect(int blockNumber) const;
    QPoint getContentOffset() const;

    // TODO: для подсветки сделать API, чтобы только к видимой части была подсветка

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    // TODO: wheel, focus и другие методы реализовать

private slots:
    void onDocumentChanged();

private:
    void updateScrollbars();
    // конвекторы позиций
    QPoint positionToLineCol(int position) const;
    int lineColToPosition(int line, int col) const;

    DocumentModel *m_model;

    int m_lineHeight; // высота строки в пикселях
    int m_charWidth; // средняя ширина символов (для моноширинных)
    int m_topMargin; // отступы сверху

    int m_cursorLine;
    int m_cursorCol;

    // TODO: данные для подсветок, выделений и тп
};

#endif

// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#include "virtualcodeeditor.h"
#include <QPainter>
#include <QScrollBar>
#include <QDebug>
#include <QPaintEvent>

VirtualCodeEditor::VirtualCodeEditor(DocumentModel *model, QWidget *parent)
    : QAbstractScrollArea(parent)
    , m_model(model)
    , m_topMargin(2)
    , m_cursorLine(0)
    , m_cursorCol(0)
{
    setFont(QFont("Fira Code", 12));
    QFontMetrics fm(font());
    m_lineHeight = fm.height();
    m_charWidth = fm.horizontalAdvance(' '); // все символы одной ширины

    // перерисовка при изменении файла
    connect(m_model, &DocumentModel::documentChanged, this, &VirtualCodeEditor::onDocumentChanged);
    onDocumentChanged(); // для первой настройки
}

// каждый раз, когда документ изменяется
void VirtualCodeEditor::onDocumentChanged()
{
    updateScrollbars();
    viewport()->update(); // планируем полную перерисовку
}

// главный метод для всей отрисовки!!!
void VirtualCodeEditor::paintEvent(QPaintEvent *event)
{
    QPainter painter(viewport());
    painter.fillRect(event->rect(), palette().color(QPalette::Base)); // заливку фона делаем

    if (m_lineHeight <= 0) {
        return; // когда высота нулевая, шрифт допустим не загрузился
    }
    // определяем первую строку на основе значения скроллбара
    int firstLine = firstVisibleBlockNumber();
    // количество помещаемых строк на экране
    int numVisibleLines = visibleBlockCount(); // запас в пару строк

    // смещение по у для первой строки
    int y = m_topMargin - (verticalScrollBar()->value() % m_lineHeight);

    // рисуем видимые строки
    for (int i = 0; i < numVisibleLines; ++i) {
        int lineIndex = firstLine + i;
        if (lineIndex >= m_model->lineCount()) {
            break; // если строки закончились, то выходим
        }

        // курсор - просто прямоугольник
        if (lineIndex == m_cursorLine) {
            painter.fillRect(m_cursorCol * m_charWidth, y, 2, m_lineHeight, palette().color(QPalette::WindowText));
        }

        // получаем текст строки
        QString line = m_model->lineAt(lineIndex);

        // рисуем текст
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(0, y, viewport()->width(), m_lineHeight, Qt::AlignLeft, line);

        y += m_lineHeight;
    }
}

// обновляем диапозоны скроллбара
void VirtualCodeEditor::updateScrollbars()
{
    if (m_lineHeight <= 0) {
        return;
    }

    // максимальная высота вертикального скролла - общая высота минус видимая область (чтобы не спустится в пустоту)
    int totalHeight = m_model->lineCount() * m_lineHeight + 2 * m_topMargin;
    verticalScrollBar()->setRange(0, qMax(0, totalHeight - viewport()->height()));
    verticalScrollBar()->setPageStep(viewport()->height()); // один шаг = видимые строки
    verticalScrollBar()->setSingleStep(m_lineHeight); // клик на пустоту скроллбара = одна строка

    // TODO: горизонтальный скролл полностью реализовать
    horizontalScrollBar()->setRange(0, 0);
}

void VirtualCodeEditor::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollbars();
}

// !!! обработка ввода !!!
void VirtualCodeEditor::mousePressEvent(QMouseEvent *event)
{
    if (m_lineHeight <= 0 || m_charWidth <= 0) {
        return; // избегаем деления на ноль
    }
    // расчет строки и позиции по координатам
    m_cursorLine = (event->y() + verticalScrollBar()->value() - m_topMargin) / m_lineHeight;
    m_cursorCol = event->x() / m_charWidth;

    // ограничение значений
    m_cursorLine = qBound(0, m_cursorLine, m_model->lineCount() - 1);
    m_cursorCol = qBound(0, m_cursorCol, m_model->lineAt(m_cursorLine).length());
    m_cursorCol = 0;

    viewport()->update(); // перерисовать, чтобы показать курсор
}

void VirtualCodeEditor::mouseMoveEvent(QMouseEvent *event)
{
    // TODO: реализовать
}

void VirtualCodeEditor::keyPressEvent(QKeyEvent *event)
{
    // TODO: полноценно реалзизовать редактирование, пока что только скролл для показа

    QPoint p = {m_cursorLine, m_cursorCol};

    switch (event->key()) {
        case Qt::Key_Up:
            p.setX(qMax(0, p.x() - 1));
            break;
        case Qt::Key_Down:
            p.setX(qMin(m_model->lineCount() - 1, p.x() + 1));
            break;
        // TODO: left, right
        default:
            QAbstractScrollArea::keyPressEvent(event);
            return;
    }

    m_cursorLine = p.x();
    m_cursorCol = qMin(p.y(), m_model->lineAt(m_cursorLine).length());

    viewport()->update();
    event->accept();
}

// !!! методы для интеграции

int VirtualCodeEditor::getCursorPosition() const
{
    // TODO: реализовать
    return 0;
}

void VirtualCodeEditor::setCursorPosition(int position)
{
    // TODO: реализовать
}

QTextCursor VirtualCodeEditor::textCursor() const
{
    // TODO: реализовать эмуляцию
    return QTextCursor();
}

void VirtualCodeEditor::setTextCursor(const QTextCursor& cursor)
{
    // TODO: реализовать
}

int VirtualCodeEditor::firstVisibleBlockNumber() const
{
    if (m_lineHeight == 0) {
        return 0;
    }
    return verticalScrollBar()->value() / m_lineHeight;
}

int VirtualCodeEditor::visibleBlockCount() const
{
    if (m_lineHeight == 0) {
        return 0;
    }
    return viewport()->height() / m_lineHeight + 2;
}

QRect VirtualCodeEditor::getBlockBoundingRect(int blockNumber) const
{
    int y = (blockNumber * m_lineHeight) - verticalScrollBar()->value() + m_topMargin;
    return QRect(0, y, viewport()->width(), m_lineHeight);
}

QPoint VirtualCodeEditor::getContentOffset() const
{
    // в модели смещение уже учтено в расчете для у в paintEvent
    return QPoint(0, 0);
}

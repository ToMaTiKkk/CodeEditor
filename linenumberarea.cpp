// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#include "linenumberarea.h"
#include <QPainter>
#include <QTextBlock>
#include <QPaintEvent>
#include <QScrollBar>
#include <QToolTip>

LineNumberArea::LineNumberArea(CodePlainTextEdit *editor) : QWidget(editor), codeEditor(editor), m_currentDigits(1) // при пердачи редактора как родителя - значит нумерация будет внутри редактора
{
    updateLineNumberAreaWidth(); // рассчитать и установить начальную ширину, даже если 0 строк

    setMouseTracking(true);
    connect(codeEditor, &QPlainTextEdit::cursorPositionChanged, this, QOverload<>::of(&LineNumberArea::update)); // подключение к изменению курсора редактора для подствеотки строки
    connect(codeEditor->document(), &QTextDocument::contentsChange, this, QOverload<>::of(&LineNumberArea::update)); // чтобы подсветка обновлялась при вставки или удалении строк
    connect(codeEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, QOverload<>::of(&LineNumberArea::update)); // обновлении при измнении видимой области
}

// вычисление необходимого количества цифр
int calculateDigits(int maxNumber) {
    int digits = 1;
    int max = qMax(1, maxNumber);
    while (max >= 10) {
        max /= 10;
        digits++;
    }
    return digits;
}

// вычисление ширины на основании количества цифр
int LineNumberArea::calculateWidth(int digits) const
{
    int padding = 10; // слева и справа отступы по 5 пикселей
    QString maxWidthString(digits, QLatin1Char('9')); // считаем ширину строки digits когда все девятки (самые широкие цифры)
    int textWidth = fontMetrics().horizontalAdvance(maxWidthString);
    return padding + textWidth +m_markerAreaWidth;
}

// обновление ширины
void LineNumberArea::updateLineNumberAreaWidth()
{
    int newDigits = calculateDigits(codeEditor->blockCount()); // считаем реально количество цифр
    const int min_digits_for_width = 3; // устнавливаем минимульное количество разрядов для расчета ширины
    if (newDigits < min_digits_for_width) {
        newDigits = min_digits_for_width;
    }

    // обновляем ширину, если кол во цифр изменилось
    if (newDigits != m_currentDigits) {
        m_currentDigits = newDigits;
        setFixedWidth(calculateWidth(m_currentDigits));
    }
}

// частиное обновление области номеров
void LineNumberArea::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        scroll(0, dy); // прокрутка области номеров
    else
        update(0, rect.y(), width(), rect.height()); // частиное обновление
}

void LineNumberArea::setDiagnotics(const QMap<int, int>& diagnostics, const QMap<int, QStringList>& diagnosticsMessage)
{
    m_diagnostics = diagnostics;
    m_diagnosticsMessage = diagnosticsMessage;
    update(); // перерисовываем
}

// метод самой отрисовки
void LineNumberArea::paintEvent(QPaintEvent *event)
{
    // будет рисовать НА нашем виджете
    QPainter painter(this);
    //painter.fillRect(event->rect(), Qt::lightGray); // фон области номеров строк
    QColor bgColor = this->palette().color(QPalette::Window);// для двух тем берет фон основного окна
    painter.fillRect(event->rect(), bgColor);
    painter.setPen(bgColor == QColor(34, 33, 50) ? QColor(92, 84, 155) : QColor(0, 123, 255));
    painter.drawLine(width() - 0.3, 0, width() - 0.3, height());
    
    // получение первого видимого блока-строки
    QTextBlock block = codeEditor->getFirstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(codeEditor->getBlockBoundingGeometry(block).translated(codeEditor->getContentOffset()).top());
    int bottom = top + qRound(codeEditor->getBlockBoundingRect(block).height());
    int currentBlockNumber = codeEditor->textCursor().blockNumber(); // получаем номер текущего блока, где курсор

    // рисуем номера видимых блоков, проходимся по кождому блоку и если их верхняя граница внутри или выше границы области перерисовки
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            int blockHeight = qRound(codeEditor->getBlockBoundingRect(block).height());
            // рисуем маркер диагностики если он есть
            if (m_diagnostics.contains(blockNumber)) {
                int severity = m_diagnostics.value(blockNumber);
                QColor color;
                if (severity == 1) {
                    color = QColor(255, 0, 0, 180); // красный, но не 100% непрозрачный
                } else if (severity == 2) {
                    color = QColor(255, 165, 0, 180); // янтарый
                } else {
                    color = QColor(0, 123, 255, 160); // синий для инфо
                }
                // вертикальная линий
                painter.setPen(QPen(color, 2));
                // рисуем линию по центру маркера на всю ширину строка
                painter.drawLine(m_markerAreaWidth / 2, top, m_markerAreaWidth / 2, top + blockHeight);
            }
            
            // рисуем сам номер строки
            QString number = QString::number(blockNumber + 1);
            painter.setFont(codeEditor->font());
            //painter.setPen(Qt::black);

            // выделение текущей строки
            if (blockNumber == currentBlockNumber) {
                // устанавливаем цвет текста для текущей строки
                //painter.setPen(palette().highlightedText().color()); плохо видно, но вроде имеет место быть
                // !!!!!! можно добавить заливку фона чутка другую, чтобы ещё более выделено было, на усмотрение!!!!!
                //QColor bgColor = palette().color(QPalette::Window); // для тем
                if (bgColor == QColor(34, 33, 50)) {
                    painter.setPen(Qt::magenta);
                } else {
                    painter.setPen(Qt::blue);
                }
            } else {
                painter.setPen(palette().windowText().color());
            }
            painter.drawText(0, top, width() - 5, blockHeight, Qt::AlignRight | Qt::AlignVCenter, number); // рисуем правее, также вертикально выравнивание по центру
        }

        // переходим к следующему блоку
        block = block.next();
        top = bottom;
        bottom = top + qRound(codeEditor->getBlockBoundingRect(block).height());
        ++blockNumber;
    }
}

void LineNumberArea::mouseMoveEvent(QMouseEvent* event)
{
    // вычисляем строку по y
    QTextCursor cursor = codeEditor->cursorForPosition(QPoint(0, event->pos().y()));
    int line = cursor.block().blockNumber();
    
    if (m_diagnosticsMessage.contains(line)) {
        QString tooltip = m_diagnosticsMessage.value(line).join("\n");
        QToolTip::showText(event->globalPos(), tooltip, this);
    } else {
        QToolTip::hideText();
        event->ignore();
    }
    QWidget::mouseMoveEvent(event);
}

#include "linenumberarea.h"
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>

LineNumberArea::LineNumberArea(QPlainTextEdit *editor) : QWidget(editor), codeEditor(editor)
{
    updateLineNumberAreaWidth(0);
}

int LineNumberArea::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, codeEditor->blockCount()); // количество строк
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 3 + codeEditor->fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void LineNumberArea::updateLineNumberAreaWidth(int newBlockCount)
{
    Q_UNUSED(newBlockCount);
    setFixedWidth(lineNumberAreaWidth()); // установка  фиксированной ширины
}

void LineNumberArea::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        scroll(0, dy); // прокрутка области номеров
    else
        update(0, rect.y(), width(), rect.height()); // частиное обновление
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), Qt::lightGray); // фон области номеров строк

    // получение первого видимого блока-строки
    QTextCursor cursor = codeEditor->cursorForPosition(QPoint(0, 0));
    QTextBlock block = cursor.block();
    int blockNumber = block.blockNumber();
    QRectF rect = codeEditor->cursorRect(cursor);
    int top = rect.top();
    int bottom = top + rect.height();

    // рисуем номера видимых блоков
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, width() - 3, codeEditor->fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        if (block.isValid()) {
            cursor.setPosition(block.position());
            rect = codeEditor->cursorRect(cursor);
            top = bottom;
            bottom = top + rect.height();
            ++blockNumber;
        } else {
            break;
        }
    }
}

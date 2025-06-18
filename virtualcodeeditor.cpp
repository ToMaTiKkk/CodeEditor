#include "virtualcodeeditor.h"
#include <QPainter>
#include <QScrollBar>
#include <QDebug>

VirtualCodeEditor::VirtualCodeEditor(DocumentModel *model, QWidget *parent)
    : QAbstractScrollArea(parent),
    m_model(model),
    m_topMargin(2),
    m_cursorLine(0),
    m_cursorCol(0)
{
    setFont(QFont("Fira Code", 12));
    QFontMetrics fm(font());
    m_lineHeight = fm.height();
    m_charWidth = fm.horizontalAdvance(' ');

    connect(m_model, &DocumentModel::documentChanged, this, &VirtualCodeEditor::onDocumentChanged);
    onDocumentChanged(); // Вызываем для первоначальной настройки
}

void VirtualCodeEditor::onDocumentChanged()
{
    updateScrollbars();
    viewport()->update();
}

void VirtualCodeEditor::paintEvent(QPaintEvent *event)
{
    QPainter painter(viewport());
    painter.fillRect(event->rect(), palette().color(QPalette::Base));

    // Ключевая проверка! Если высота строки 0, рисовать нечего.
    if (m_lineHeight <= 0) {
        return;
    }

    int firstLine = verticalScrollBar()->value() / m_lineHeight;
    int numVisibleLines = viewport()->height() / m_lineHeight + 2; // +2 для частичной прорисовки

    int y = m_topMargin - (verticalScrollBar()->value() % m_lineHeight);

    for (int i = 0; i < numVisibleLines; ++i) {
        int lineIndex = firstLine + i;
        if (lineIndex >= m_model->lineCount()) {
            break;
        }

        if (lineIndex == m_cursorLine) {
            painter.fillRect(m_cursorCol * m_charWidth, y, 2, m_lineHeight, palette().color(QPalette::WindowText));
        }

        QString line = m_model->lineAt(lineIndex);
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(0, y, viewport()->width(), m_lineHeight, Qt::AlignLeft, line);

        y += m_lineHeight;
    }
}

void VirtualCodeEditor::updateScrollbars()
{
    // Такая же проверка здесь
    if (m_lineHeight <= 0) return;

    int totalHeight = m_model->lineCount() * m_lineHeight + 2 * m_topMargin;
    verticalScrollBar()->setRange(0, qMax(0, totalHeight - viewport()->height()));
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setSingleStep(m_lineHeight);

    horizontalScrollBar()->setRange(0, 0);
}

void VirtualCodeEditor::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollbars();
}

// ... остальной код (mousePressEvent и т.д.) без изменений из первой версии ...
// (mousePress, mouseMove, keyPress, и пустые заглушки для API)
// Я добавлю их сюда для полноты, чтобы ты мог просто скопировать весь файл

void VirtualCodeEditor::mousePressEvent(QMouseEvent *event)
{
    // Проверка, чтобы избежать деления на ноль
    if (m_lineHeight <= 0 || m_charWidth <= 0) return;

    m_cursorLine = (event->y() + verticalScrollBar()->value() - m_topMargin) / m_lineHeight;
    m_cursorCol = event->x() / m_charWidth;

    m_cursorLine = qBound(0, m_cursorLine, m_model->lineCount() - 1);
    m_cursorCol = qBound(0, m_cursorCol, m_model->lineAt(m_cursorLine).length());

    viewport()->update();
}

void VirtualCodeEditor::mouseMoveEvent(QMouseEvent *event)
{
    // TODO
}

void VirtualCodeEditor::keyPressEvent(QKeyEvent *event)
{
    QPoint p = {m_cursorLine, m_cursorCol};

    switch (event->key()) {
    case Qt::Key_Up:
        p.setX(qMax(0, p.x() - 1));
        break;
    case Qt::Key_Down:
        p.setX(qMin(m_model->lineCount() - 1, p.x() + 1));
        break;
    // TODO: Добавить Left/Right
    default:
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

    m_cursorLine = p.x();
    m_cursorCol = qMin(p.y(), m_model->lineAt(m_cursorLine).length());

    viewport()->update();
    event->accept();
}

int VirtualCodeEditor::getCursorPosition() const { return 0; }
void VirtualCodeEditor::setCursorPosition(int position) {}
QTextCursor VirtualCodeEditor::textCursor() const { return QTextCursor(); }
void VirtualCodeEditor::setTextCursor(const QTextCursor& cursor) {}
int VirtualCodeEditor::firstVisibleBlockNumber() const { return (m_lineHeight > 0) ? (verticalScrollBar()->value() / m_lineHeight) : 0; }
int VirtualCodeEditor::visibleBlockCount() const { return (m_lineHeight > 0) ? (viewport()->height() / m_lineHeight + 1) : 0; }
QRect VirtualCodeEditor::getBlockBoundingRect(int blockNumber) const { int y = (blockNumber * m_lineHeight) - verticalScrollBar()->value() + m_topMargin; return QRect(0, y, viewport()->width(), m_lineHeight); }
QPoint VirtualCodeEditor::getContentOffset() const { return QPoint(0, 0); }

#ifndef VIRTUALCODEEDITOR_H
#define VIRTUALCODEEDITOR_H

#include <QAbstractScrollArea>
#include <QFont>
#include <QTextCursor> // Для эмуляции
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include "documentmodel.h"

// VirtualCodeEditor - наш новый, полностью виртуализированный редактор.
// Он наследуется от QAbstractScrollArea, что дает нам готовые скроллбары и viewport.
// Наша задача - правильно отрисовывать текст в paintEvent.
class VirtualCodeEditor : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit VirtualCodeEditor(DocumentModel *model, QWidget *parent = nullptr);

    // Методы для интеграции с остальной частью IDE
    int getCursorPosition() const;
    void setCursorPosition(int position);
    QTextCursor textCursor() const; // Эмуляция QTextCursor
    void setTextCursor(const QTextCursor& cursor);

    // Для LineNumberArea
    int firstVisibleBlockNumber() const;
    int visibleBlockCount() const;
    QRect getBlockBoundingRect(int blockNumber) const;
    QPoint getContentOffset() const;

    // Для подсветки
    // В будущем тут будет API для получения видимых блоков
    // и применения к ним правил подсветки.

protected:
    // Ключевые переопределенные методы
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    // ... другие события (wheel, focus, etc.)

private slots:
    // Слот для реакции на изменения в модели
    void onDocumentChanged();

private:
    void updateScrollbars();
    // Конвертация между позициями
    QPoint positionToLineCol(int position) const;
    int lineColToPosition(int line, int col) const;


    DocumentModel *m_model; // Указатель на нашу модель данных

    int m_lineHeight;      // Высота одной строки в пикселях
    int m_charWidth;       // Средняя ширина символа (для моноширинных шрифтов)
    int m_topMargin;       // Отступ сверху

    // Состояние курсора
    int m_cursorLine;
    int m_cursorCol;

    // TODO: Здесь будут храниться данные для подсветки, выделения и т.д.
};

#endif // VIRTUALCODEEDITOR_H

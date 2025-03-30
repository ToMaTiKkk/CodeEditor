#ifndef LINENUMBERAREA_H
#define LINENUMBERAREA_H

#include <QWidget>
#include <QPlainTextEdit>

class LineNumberArea : public QWidget
{
    Q_OBJECT

public:
    LineNumberArea(QPlainTextEdit *editor); // привязка к конкретному редактору

    int lineNumberAreaWidth() const; // метод для получения рекомендуемой ширины нумерации, через конст не изменяет состояние объекта

protected:
    void paintEvent(QPaintEvent *event) override; // переопределение, отрисовка нумерации

public slots:
    void updateLineNumberAreaWidth(int newBlockCount); // когда меняется колво строк в редакторе, то обновляется ширина виджета
    void updateLineNumberArea(const QRect &rect, int dy); // при скролле или измнении текста в редакторе перерисовка

private:
    QPlainTextEdit *codeEditor;
};

#endif

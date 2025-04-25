#ifndef LINENUMBERAREA_H
#define LINENUMBERAREA_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPalette>

class LineNumberArea : public QWidget
{
    Q_OBJECT

public:
    LineNumberArea(QPlainTextEdit *editor); // привязка к конкретному редактору

    // ширина области (цифры + отступы + маркеры)
    int calculateWidth(int digits) const;
    //int lineNumberAreaWidth() const; // метод для получения рекомендуемой ширины нумерации, через конст не изменяет состояние объекта
    void setDiagnotics(const QMap<int, int>& diagnotics);

protected:
    void paintEvent(QPaintEvent *event) override; // переопределение, отрисовка нумерации

public slots:
    void updateLineNumberAreaWidth(); // когда меняется колво строк в редакторе, то обновляется ширина виджета
    void updateLineNumberArea(const QRect &rect, int dy); // при скролле или измнении текста в редакторе перерисовка

private:
    QPlainTextEdit *codeEditor;
    int m_currentDigits; // хранение количества цифр, под которое рассчитана ширина

    // номер строки -> уровень срьезности (1 - ошибка, 2 - предупреждение, 3 - инфо)
    QMap<int, int> m_diagnostics;
    // ширина мини-зоны маркеров
    const int m_markerAreaWidth = 4;
};

#endif

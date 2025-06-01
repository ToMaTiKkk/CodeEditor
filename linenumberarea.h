// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LINENUMBERAREA_H
#define LINENUMBERAREA_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPalette>
#include <QMouseEvent>

class LineNumberArea : public QWidget
{
    Q_OBJECT

public:
    LineNumberArea(QPlainTextEdit *editor); // привязка к конкретному редактору

    // ширина области (цифры + отступы + маркеры)
    int calculateWidth(int digits) const;
    //int lineNumberAreaWidth() const; // метод для получения рекомендуемой ширины нумерации, через конст не изменяет состояние объекта
    void setDiagnotics(const QMap<int, int>& diagnotics, const QMap<int, QStringList>& diagnosticsMessage);

protected:
    void paintEvent(QPaintEvent *event) override; // переопределение, отрисовка нумерации
    void mouseMoveEvent(QMouseEvent* event) override; // для тултипа при наведении слева на диагностику

public slots:
    void updateLineNumberAreaWidth(); // когда меняется колво строк в редакторе, то обновляется ширина виджета
    void updateLineNumberArea(const QRect &rect, int dy); // при скролле или измнении текста в редакторе перерисовка

private:
    QPlainTextEdit *codeEditor;
    int m_currentDigits; // хранение количества цифр, под которое рассчитана ширина

    // номер строки -> уровень срьезности (1 - ошибка, 2 - предупреждение, 3 - инфо)
    QMap<int, int> m_diagnostics;
    QMap<int, QStringList> m_diagnosticsMessage; // хранение сообщение для каждой из диагностик
    // ширина мини-зоны маркеров
    const int m_markerAreaWidth = 4;
    
};

#endif

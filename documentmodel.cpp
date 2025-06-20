// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#include "documentmodel.h"
#include <QTextStream>
#include <QIODevice>

DocumentModel::DocumentModel(QObject *parent) : QObject(parent)
{
    m_lines.append(""); // пустой документ - это одна пустая строка, но она есть
}

int DocumentModel::lineCount() const
{
    return m_lines.size();
}

QString DocumentModel::lineAt(int index) const
{
    if (index >= 0 && index < lineCount()) {
        return m_lines.at(index);
    }
    return QString();
}

const QVector<QString>& DocumentModel::allLines() const
{
    return m_lines;
}

void DocumentModel::insertLine(int index, const QString& line)
{
    if (index >= 0 && index <= lineCount()) {
        m_lines.insert(index, line);
        emit documentChanged();
    }
}

void DocumentModel::removeLine(int index)
{
    // нельзя удалить последнюю строку, одна любая всегда должна быть
    if (index >= 0 && index < lineCount() && lineCount() > 1) {
        m_lines.remove(index);
        emit documentChanged();
    }
}

void DocumentModel::replaceLine(int index, const QString& line)
{
    if (index >= 0 && index <= lineCount()) {
        m_lines[index] = line;
        emit documentChanged();
    }
}

// загружает весь текст, деля на строки
// идеально для первой загрузки и для небольших файлов
void DocumentModel::setContent(const QString& content)
{
    m_lines.clear();
    m_lines = content.split('\n', Qt::KeepEmptyParts); // обработка с пустыми строка в конце
    emit documentChanged();
}

// оптимизированный подход для добавления больших кусков текста
// обрабатывает незавершенные строки на границах чанков
void DocumentModel::appendChunk(const QString& chunk)
{
    if (chunk.isEmpty()) {
        return;
    }

    QString dataToProcess;
    // если есть незавершенная строка, то присоединяем к ней начало нового чанка
    if (!m_lines.isEmpty()) {
        dataToProcess = m_lines.last() + chunk;
        m_lines.pop_back();
    } else {
        dataToProcess = chunk;
    }

    QStringList newLines = dataToProcess.split('\n', Qt::KeepEmptyParts);
    for (const QString& line : newLines) {
        m_lines.append(line);
    }

    // если исходный чанк без \n на конце, то строка - неполная, QTextStream отсавляет ее в векторе и мы ее сохраняем для следующего чанка

    emit documentChanged();
}

void DocumentModel::clear()
{
    if (lineCount() > 1 || !m_lines.first().isEmpty()) {
        m_lines.clear();
        m_lines.append(""); // всегда пустая строка должна всегда
        emit documentChanged();
    }
}

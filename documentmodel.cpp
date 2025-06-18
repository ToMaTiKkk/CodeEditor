#include "documentmodel.h"
#include <QTextStream>

// В конструкторе сразу добавляем одну пустую строку.
// Это гарантирует, что lineCount() никогда не будет 0 для валидного документа.
DocumentModel::DocumentModel(QObject *parent) : QObject(parent)
{
    m_lines.append("");
}

int DocumentModel::lineCount() const
{
    return m_lines.size();
}

QString DocumentModel::lineAt(int index) const
{
    if (index >= 0 && index < m_lines.size()) {
        return m_lines.at(index);
    }
    return QString();
}

// Заменяем на allLines(), чтобы избежать путаницы с documentText()
const QVector<QString>& DocumentModel::allLines() const
{
    return m_lines;
}

void DocumentModel::insertLine(int index, const QString& line)
{
    if (index >= 0 && index <= m_lines.size()) {
        m_lines.insert(index, line);
        emit documentChanged();
    }
}

void DocumentModel::removeLine(int index)
{
    // Нельзя удалять последнюю строку, документ должен содержать хотя бы одну.
    if (index >= 0 && index < m_lines.size() && m_lines.size() > 1) {
        m_lines.remove(index);
        emit documentChanged();
    }
}

void DocumentModel::replaceLine(int index, const QString& line)
{
    if (index >= 0 && index < m_lines.size()) {
        m_lines[index] = line;
        emit documentChanged();
    }
}

void DocumentModel::setContent(const QString& content)
{
    m_lines.clear();
    // Используем split с KeepEmptyParts, чтобы правильно обрабатывать пустые строки в конце.
    m_lines = content.split('\n', Qt::KeepEmptyParts);

    // Если контент был пустой, split вернет одну пустую строку, что нам и нужно.
    // Если контент был "text", split вернет одну строку "text".
    // Если контент был "text\n", split вернет ["text", ""], что тоже правильно.

    emit documentChanged();
}

void DocumentModel::appendChunk(const QString& chunk)
{
    if (chunk.isEmpty()) return;

    QString dataToProcess;
    if (!m_lines.isEmpty()) {
        dataToProcess = m_lines.last() + chunk;
        m_lines.pop_back();
    } else {
        dataToProcess = chunk;
    }

    // Здесь тоже используем split для надежности
    QStringList newLines = dataToProcess.split('\n', Qt::KeepEmptyParts);
    for(const QString& line : newLines) {
        m_lines.append(line);
    }

    emit documentChanged();
}

void DocumentModel::clear()
{
    if (m_lines.size() > 1 || !m_lines.first().isEmpty()) {
        m_lines.clear();
        m_lines.append(""); // Всегда оставляем одну пустую строку
        emit documentChanged();
    }
}

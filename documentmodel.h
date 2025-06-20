// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOCUMENTMODEL_H
#define DOCUMENTMODEL_H

#include <QObject>
#include <QString>
#include <QVector>

// данный класс хранит все строки документа, но их отображением НЕ ЗАНИМАЕТСЯ
// он лишь дает интерфейс для доступа к данным и их изменениям
class DocumentModel : public QObject
{
    Q_OBJECT

public:
    explicit DocumentModel(QObject *parent = nullptr);

    // интерфейс для доступа к данным
    int lineCount() const;
    QString lineAt(int index) const;
    const QVector<QString>& allLines() const; // для сохрана и начальной загрузки

    // интерфейс для изменения данных
    void insertLine(int index, const QString& line);
    void removeLine(int index);
    void replaceLine(int index, const QString& line);
    void setContent(const QString& content); // установить все содержимое из одной большой строки
    void appendChunk(const QString& chunk); // добавление данных по кускам
    void clear();

signals:
    // сигнал об изменении документа, отправляется всей программе и всем системам, такие как допустим LSP, что нужно обновится
    void documentChanged();

private:
    QVector<QString> m_lines; // храним документ как вектор строк для эффективности доступа по номеру строки
};

#endif

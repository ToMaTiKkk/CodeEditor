// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LARGEFILELOADER_H
#define LARGEFILELOADER_H

#include "documentmodel.h"
#include <QObject>
#include <QPlainTextEdit>
#include <QFile>
#include <QTimer>

class LargeFileLoader : public QObject
{
    Q_OBJECT
public:
    explicit LargeFileLoader(DocumentModel *model, QObject *parent = nullptr);
    // главная функция загрузки
    void loadFile(const QString &filePath);
    QString loadedText() const; // получение текста

signals:
    // сигнал о завершении загрузки
    void loadingFinished(bool success, const QString &errorString);
    void progressChanged(int percentage); // индикатор для обновления прогресса, например для Progressbar

private slots:
    // по таймеру будет вызываться для следующей загрузки чанка
    void loadNextChunk();

private:
    DocumentModel *m_model;
    QString m_loadedText;
    QFile m_file;
    QTimer m_timer;
    QTextStream *m_textStream;
    qint64 m_fileSize;
    const qint64 m_chunkSize = 1024 * 1024; // по 1 Мб за раз
};

#endif

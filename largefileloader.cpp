// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#include "largefileloader.h"
#include <QTextStream>
#include <QSignalBlocker>
#include <QCoreApplication>

LargeFileLoader::LargeFileLoader(DocumentModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_textStream(nullptr)
    , m_fileSize(0)
{
    m_timer.setInterval(0); // ASYNC выполнить как можно скорее, но после обработки других событий
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &LargeFileLoader::loadNextChunk);
}

void LargeFileLoader::loadFile(const QString &filePath)
{
    if (m_file.isOpen()) {
        m_file.close();
    }

    // перед загрузкой всегда очищаем модель
    if (m_model) {
        m_model->clear();
    }

    m_file.setFileName(filePath);
    if (!m_file.open(QFile::ReadOnly | QFile::Text)) {
        // не удалось открыть
        emit loadingFinished(false, m_file.errorString());
        return;
    }

    m_fileSize = m_file.size();

    m_timer.start(); // первый чанк запускаем
}

void LargeFileLoader::loadNextChunk()
{
    if (!m_file.isOpen() || m_file.atEnd()) {
        if (m_file.isOpen()) {
            m_file.close();
        }
        // успешная загрузка
        emit loadingFinished(true, "");
        return;
    }

    QByteArray chunkBytes = m_file.read(m_chunkSize);
    QString chunk = QString::fromUtf8(chunkBytes);

    // чанк в модель добавляем
    if (m_model) {
        m_model->appendChunk(chunk);
    }

    // обновление прогресса загрузки
    qint64 currentPos = m_file.pos();
    if (m_fileSize > 0){
        emit progressChanged(static_cast<int>((currentPos * 100) / m_fileSize));
    }

    // даем UI время на обработку событий (перерисовку и тп) чтобы не было зависаний
    QCoreApplication::processEvents();
    m_timer.start();
}

QString LargeFileLoader::loadedText() const
{
    return "This method not supported!!! Need delete. Get data from DocumentModel";
}

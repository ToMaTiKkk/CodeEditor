#include "largefileloader.h"
#include <QTextStream>
#include <QSignalBlocker>
#include <QCoreApplication>

LargeFileLoader::LargeFileLoader(QPlainTextEdit *editor, QObject *parent)
    : QObject(parent)
    , m_editor(editor)
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
        delete m_textStream;
        m_textStream = nullptr;
    }

    m_file.setFileName(filePath);
    if (!m_file.open(QFile::ReadOnly | QFile::Text)) {
        // не удалось открыть
        emit loadingFinished(false, m_file.errorString());
        return;
    }

    m_fileSize = m_file.size();
    m_textStream = new QTextStream(&m_file);

    m_loadedText.clear();
    m_editor->clear();
    m_editor->document()->blockSignals(true); // блокируем отправку сигналов в лсп в contentsChange
    m_timer.start(); // первый чанк запускаем
}

void LargeFileLoader::loadNextChunk()
{
    if (!m_file.isOpen() || m_file.atEnd()) {
        m_editor->document()->blockSignals(false);
        m_file.close();
        delete m_textStream;
        m_textStream = nullptr;
        emit loadingFinished(true, "");
        return;
    }

    QString chunk = m_textStream->read(m_chunkSize);
    // вставка в конец документа
    QTextCursor cursor(m_editor->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(chunk);
    m_loadedText.append(chunk);

    // обновление прогресса загрузки
    qint64 currentPos = m_file.pos();
    emit progressChanged(static_cast<int>((currentPos * 100) / m_fileSize));

    // даем UI время на обработку событий (перерисовку и тп) чтобы не было зависаний
    QCoreApplication::processEvents();
    m_timer.start();
}

QString LargeFileLoader::loadedText() const
{
    return m_loadedText;
}

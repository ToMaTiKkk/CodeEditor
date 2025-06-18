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

    // Перед загрузкой нового файла очищаем модель
    if (m_model) {
        m_model->clear();
    }

    m_file.setFileName(filePath);
    if (!m_file.open(QFile::ReadOnly | QFile::Text)) {
        emit loadingFinished(false, m_file.errorString());
        return;
    }

    m_fileSize = m_file.size();

    // Запускаем первый чанк
    m_timer.start();
}

void LargeFileLoader::loadNextChunk()
{
    // Если файл закончился, завершаем процесс
    if (!m_file.isOpen() || m_file.atEnd()) {
        if(m_file.isOpen()) m_file.close();
        // Успешно завершили загрузку
        emit loadingFinished(true, "");
        return;
    }

    // Читаем чанк из файла
    QByteArray chunkBytes = m_file.read(m_chunkSize);
    QString chunk = QString::fromUtf8(chunkBytes); // Важно использовать правильную кодировку

    // Добавляем чанк в нашу модель. Вся магия происходит там.
    if (m_model) {
        m_model->appendChunk(chunk);
    }

    // Обновляем прогресс
    qint64 currentPos = m_file.pos();
    if (m_fileSize > 0) {
        emit progressChanged(static_cast<int>((currentPos * 100) / m_fileSize));
    }

    // Даем UI время на обработку событий. Это предотвращает "залипание" интерфейса во время загрузки.
    QCoreApplication::processEvents();
    // Запускаем таймер для следующего чанка
    m_timer.start();
}

QString LargeFileLoader::loadedText() const
{
    return "This method is deprecated. Get data from DocumentModel.";
}

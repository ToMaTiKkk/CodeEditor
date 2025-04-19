#include "lspmanager.h"
#include <QDebug>
#include <QJsonParseError>
#include <QUrl> // для работы с URI, чтобы преобразовать путь файла в формате file:///...
#include <QFileInfo>
#include <QCoreApplication> // чтобы получать ID нашего приложения для инициализации нужен
#include <QJsonArray>
#include <QJsonObject>
#include <QTextBlock>

LspManager::LspManager(QString serverExecutablePath, QObject *parent)
    : QObject(parent)
    , m_serverExecutablePath(serverExecutablePath)
{

}

LspManager::~LspManager()
{
    stopServer();
}

// !!! инициализация и управление процессом !!!

// запуск сервера (пример clangd)
bool LspManager::startServer(const QString& languageId, const QString& projectRootPath)
{
    // проверка а не запущен ли сервер
    if (m_lspProcess && m_lspProcess->state() != QProcess::NotRunning) {
        qWarning() << "LSP сервер уже запущен";
        return false; // запустить не удалось, так как уже запущен
    }

    if (m_serverExecutablePath.isEmpty()) {
        qCritical() << "Путь к исполняемому файлу LSP сервера не задан!";
        emit serverError("Путь к LSP серверу не задан");
        return false;
    }
    // созранение параметров
    m_languageId = languageId; // запоминаем язык
    m_rootUri = QUrl::fromLocalFile(projectRootPath).toString(); // конвектируем формат пути из "/home/user/project" в "file:///home/user/project", который требует ЛСП

    qInfo() << "Запускаем LSP сервер:" << m_serverExecutablePath << "для проекта" << m_rootUri;

    m_lspProcess = new QProcess(this); // создаем объект, который будет управлять внешним процессом, удаляется вместе с родителем, то есть с LspManager

    // настройках уведов, когда процесс напишет в stdout, то вызываем функцию
    connect(m_lspProcess, &QProcess::readyReadStandardOutput, this, &LspManager::onReadyReadStandardOutput);
    connect(m_lspProcess, &QProcess::readyReadStandardError, this, &LspManager::onReadyReadStandardError);
    connect(m_lspProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &LspManager::onProcessFinished);
    connect(m_lspProcess, &QProcess::errorOccurred, this, &LspManager::onProcessError); // ошибки при запуске или работе

    // запускаем процесс и qt найдет m_serverExecutable с системных путях в PATH
    m_lspProcess->start(m_serverExecutablePath);

    // задержка в 5 сек, чтобы точно убедиться, что процесс запустился
    if (!m_lspProcess->waitForStarted(5000)) {
        qCritical() << "Не удалось запустить процсс LSP сервера:" << m_lspProcess->errorString();
        m_lspProcess->deleteLater(); // так как не запустился
        m_lspProcess = nullptr;
        emit serverError("Не удалось запустить LSP сервер: " + m_serverExecutablePath); // посылаем в mainwindow сигнал об ошибке
        return false; // запуск не удался
    }

    qInfo() << "Процесс LSP сервера запущен";
    m_isServerReady = false; // сервер запущен, но не готов к работе, соединение не установлено с клиентом

    // !!! отправляем запрос с инициализацией, чтобы серверу сообщить, что подключился клиент !!!
    QJsonObject params;
    params["processId"] = (qint64)QCoreApplication::applicationPid(); // сообщаем наш айди редактора
    // сообщаем серверу какие функции поддерживаются нашим редактором, пока что просто заглушка!!!
    QJsonObject capabilities;
    QJsonObject windowCap;
    // сообщаем, что можем показывать сообщения от сервера
    windowCap["showMessage"] = QJsonObject{{ "messageActionItem",  QJsonObject{} }};
    // базовая поддержка
    capabilities["window"] = windowCap;

    QJsonObject textDocumentCap;
    // синхра: сообщаем, что будем слать полный текст при изменении (SyncKind.Full)
    textDocumentCap["synchronization"] = QJsonObject {
      {"dynamicRegistration", false}, // пока что нет поддержки динамической регистрации
      {"willSave", false}, // не уведомляем перед сохранением
      {"willSaveWaitUntil", false}, // не ждем ответа перед сохранением
      {"didSave", true}, // уведомляем ПОСЛЕ сохранения   
    };
    // автодополнение: сообщаем, что поддерживается бащовое автодополнение
    textDocumentCap["completion"] = QJsonObject {
        {"dynamicRegistration", false},
        {"completionItem", QJsonObject{
            {"snippetSupport", false}, // пока не поддерживаем сниппеты
            {"documentationFormat", QJsonArray{"plaintext", "markdown"}} // понимаем текст и markdown в документации
        }},
        {"contextSupport", true} // сообщаем triggerKind при запроса
    };
    // hover
    textDocumentCap["hover"] = QJsonObject {
        {"dynamicRegistration", false},
        {"linkSupport", false}, // пока не поддерживаем LocationLink
    };
    capabilities["textDocument"] = textDocumentCap;

    params["capabilities"] = capabilities; // вставляем наши возможности
    params["rootUri"] = m_rootUri; // путь к папке проекта
    params["trace"] = "off"; // уровень отладки соо

    QJsonObject message;
    message["jsonrpc"] = "2.0"; // версия протокола
    message["id"] = ++m_requestId; // уникальный айди запроса
    message["method"] = "initialize"; // имя запроса
    message["params"] = params;

    sendMessage(message);
    qDebug() << "LSP > Отправлен запрос initialize, ID:" << m_requestId;

    return true; // процесс запущен, initialize отправлен
}

void LspManager::stopServer()
{
    // проверяем что процесс есть и что он работает
    if (m_lspProcess && m_lspProcess->state() != QProcess::NotRunning) {
        qInfo() << "Останавливаем LSP сервер...";
        m_isServerReady = false; // сервер больше не готов
        m_pendingRequests.clear(); // очищаем незавершенные запросы

        // !!! отправляем shutdown и exit !!!
        // вежливо просим чтобы сервер завершил работу
        QJsonObject shutdownMsg;
        shutdownMsg["jsonrpc"] = "2.0";
        shutdownMsg["id"] = ++m_requestId; // новый айдишник запроса
        shutdownMsg["method"] = "shutdown";
        sendMessage(shutdownMsg);
        qDebug() << "LSP > Отправлен запрос на shutdown, ID:" << m_requestId; // -------------------TODO в идеале нужно дождаться ответа на это, преждем чем отправлть exit

        // уведомление, чтобы сервер точно завершился
        // QJsonObject exitMsg;
        // exitMsg["jsonrpc"] = "2.0";
        // exitMsg["method"] = "exit";
        // sendMessage(exitMsg);
        // qDebug() << "LSP ОТправлено уведомление exit";

        // даем серверу немного времени, чтобы завершиться позже, так как асинхронный шаг
        if (!m_lspProcess->waitForFinished(5000)) {
            qWarning() << "LSP сервер не завершился сам, пытаемся терминировать";
            // просим OC принудитеьно его остановить
            m_lspProcess->terminate();
            if (!m_lspProcess->waitForFinished(1000)) {
                qWarning() << "LSP сервер не терминировался, убиваем";
                // крайняя мера
                m_lspProcess->kill();
            }
        } else {
            qInfo() << "LSP сервер остановлен";
        }
    }
    // освобождаем память
    if (m_lspProcess) {
        m_lspProcess = nullptr;
    }
}

// проверяем готов ли сервер
bool LspManager::isReady() const {
    return m_isServerReady;
}

// !!! авто обработчики событий от процесса !!!
// когда сервер что то написал в stdout (прислал сообщение)
void LspManager::onReadyReadStandardOutput()
{
    if (!m_lspProcess) return; // если процесса нет
    // читаем ВСЕ данные от вывода сервера
    QByteArray data = m_lspProcess->readAllStandardOutput();
    // отправляем данные в функцию разбора сообщений
    processIncomingData(data);
}

// вызывается когда сервер написал в stderr сообщения об ошибке
void LspManager::onReadyReadStandardError()
{
    if (!m_lspProcess) return;
    QByteArray errorData = m_lspProcess->readAllStandardError();
    qWarning() << "LSP stderr:" << QString::fromUtf8(errorData);
    // ------- TODO можно клиенту послуать сигнал serverError
}

// когда процесс сервера завершился
void LspManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qInfo() << "Процесс LSP завершен. КОд выхода:" << exitCode << "Статус:" << exitStatus;
    m_isServerReady = false; // сервер больше не готов
    if (m_lspProcess) {
        m_lspProcess->deleteLater(); // в очередь событий Qt, безопаснее чем просто delete
        m_lspProcess = nullptr;
    }
    emit serverStopped(); // посылаем клиенту сигнал об остановке
}

// если произошла ошибка с самим процессом (например не найден файл анализатора (clangd))
void LspManager::onProcessError(QProcess::ProcessError error)
{
    qCritical() << "Ошибка процесса LSP:" << error << m_lspProcess->errorString();
    m_isServerReady = false;
    // посылаем клиенту сигнал с описание ошибки процесс
    if (m_lspProcess) {
        emit serverError("Ошибка процесса LSP:" + m_lspProcess->errorString());
        m_lspProcess->deleteLater();
        m_lspProcess = nullptr;
    } else
        emit serverError("Ошибка процесса LSP: Неизвестная ошибка (процесс не существует)");
}

// !!! отправка и прием сообщений (JSON-RPC) !!!
void LspManager::sendMessage(const QJsonObject& message)
{
    if (!m_lspProcess || m_lspProcess->state() != QProcess::Running) {
        qWarning() << "Не могу отправить сообщение, процесс LSP не запущен";
        return;
    }

    // конвектируем джсон-объект в массив байт (текст)
    QJsonDocument doc(message);
    QByteArray jsonContent = doc.toJson(QJsonDocument::Compact); // без лишних пробелов и переносов строк

    // формирование обязательного заголовка
    QByteArray header;
    header.append("Content-Length: ").append(QString::number(jsonContent.size()).toUtf8()).append("\r\n\r\n"); // текст заговлока? размер джсон в байтах,  обязательные символы конца заголовка, перевод строки и возврат каретки x2
    //header.append(QString::number(jsonContent.size())); // 
    //header.append("\r\n\r\n"); //

    // отправляем сначала заголовок, а потом само сообщение джсон в стандартный ввод процесса (stding)
    m_lspProcess->write(header);
    m_lspProcess->write(jsonContent);

    // сохраняем айди и метод для запросов
    if (message.contains("id") && message.contains("method")) {
        qint64 id = message["id"].toVariant().toLongLong();
        QString method = message["method"].toString();
        if (id > 0 && !method.isEmpty()) { // убедимся что запрос валидный
            m_pendingRequests.insert(id, method); 
            qDebug() << "LSP > Запрос поставлен в очередь ожидания: ID" << id << "Метод:" << method;
        }
    }
}

// обрабатывает входящие данные из буфера
void LspManager::processIncomingData(const QByteArray& data)
{
    // доюавляем только что прочитанные данные в конец буфера
    m_buffer.append(data);

    // запускаем цикл чтобы попытаться извлечь из буфера одно или несколько полных сообщений
    while (true) {
        // индекс конца заголовка ищем ("\r\n\r\n")
        int headerEndIndex = m_buffer.indexOf("\r\n\r\n");
        //если не нашли, то пришел неполный заголовок или его вообще нет и нужно следующих данных от сервера
        if (headerEndIndex == -1) {
            qDebug() << "LSP < Неполный заголовок, ждем данных";
            break;
        } 

        // заголовок найден и извлекаем его часть до "\r\n\r\n"
        QByteArray headerPart = m_buffer.left(headerEndIndex);
        int contentLength = -1; // записываем длину джсон содержимого, ищем строку с Content-Length, там длина
        QList<QByteArray> headers = headerPart.split('\n'); // построчно разделяем заголовок
        for (const QByteArray& headerLine : headers) {
            if (headerLine.startsWith("Content-Length:")) {
                // длину нашли, извлекаем, убираем пробелы и конвектируем в инт
                contentLength = headerLine.mid(15).trimmed().toInt();
                break;
            }
        }

        // проверяем нашли ли конкретную длину
        if (contentLength <= 0) {
            qWarning() << "LSP < Неверный или отсутствующий Content-Length"; // не знаем сколько байт читать для джсон и пытаемся пропустить это битое соо, удаляя данные до конца заголовка + 4 символа ("\r\n\r\n")
            m_buffer = m_buffer.mid(headerEndIndex + 4);
            continue; // чтобы искать следующий заголовок цикл продолжается
        }

        // знаем длину соо и проверяем, достаточно ли данных в буфере
        int totalMessageLength = headerEndIndex + 4 + contentLength; // общая длина соо
        if (m_buffer.size() < totalMessageLength) {
            // данных в буфере недостаточно, ждем следующих данных чтобы сообщение было полным
            qDebug() << "LSP < Неполное тело сообщения, ждем... Нужно" << totalMessageLength << "есть" << m_buffer.size();
            break;
        }

        // данных наконец достаточно, извлекает тело сообщения из буфера, начинается сразу после заголовка и иметь определнную длину
        QByteArray jsonContent = m_buffer.mid(headerEndIndex + 4, contentLength);
        // удаляем обработанное соо из начала буфера (заголовок + тело) и ещё оставляем не разобранные данные
        m_buffer = m_buffer.mid(totalMessageLength);

        // цикл начинается снова чтобы проверить, а нет ли ещё полного соо в буфере
        parseMessage(jsonContent);
    }

    qDebug() << "LSP < Закончил обработку куска данных, остаток в буфере:" << m_buffer.size();
}

// разбираем одно джсон соо
void LspManager::parseMessage(const QByteArray& jsonContent)
{
    QJsonParseError parseError; // если джсон некорректный, то записываем инфо об ошибке
    QJsonDocument doc = QJsonDocument::fromJson(jsonContent, &parseError);

    // проверяем не было ли ошибок при разборе
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "LSP < Ошибка разбора JSON:" << parseError.errorString() << "в тексте:" << QString::fromUtf8(jsonContent);
        return; // не можем разобрать и выходим
    }

    // json является объектом {...}, а не массивом [...] или просто числом или строкой
    if (!doc.isObject()) {
        qWarning() << "LSP < Полученный JSON не является объектом:" << QString::fromUtf8(jsonContent);
        return; // выходим. не тот формат
    }

    QJsonObject message = doc.object(); // для удобного доступа к полям преобразуем
    // определяем тип соо на наличие полей "id", "result", "error", "method" and etc
    if (message.contains("id") && message.contains("result") || message.contains("error")) {
        // это ответ на наш вопрос
        qint64 id = message["id"].toVariant().toLongLong();
        if (!m_pendingRequests.contains(id)) {
            qWarning() << "LSP < Получен ответ на неизвсетный или уже обработанный запрос ID:" << id;
        }
        QString method = m_pendingRequests.take(id); // полуаем метод и сразу удаляем из словаря
        if (message.contains("result")) {
            // поле результата может быть любым объектом, массивом или другим типом
            QJsonValue resultValue = message["result"];

            if (method == "initialize") {
                handleInitializeResult(resultValue.toObject());
            } else if (method == "textDocument/completion") {
                // рез может быть массивом или объектом
                if (resultValue.isObject()) {
                    handleCompletionResult(resultValue.toObject());
                } else if (resultValue.isArray()) {
                    handleCompletionResult(QJsonObject{{ "items", resultValue }}); // обернем массив в объект для единообразия
                } else if (resultValue.isNull()) {
                    handleCompletionResult({});
                } else {
                    qWarning() << "LSP < неожиданный тип результата для completion: " << resultValue.type();
                }
            } else if (method == "textDocument/definition") {
                // definition может вернуть просто Location, LOcation[] or Null
                handleDefinitionResult(resultValue.toObject());
            } else if (method == "textDocument/hover") {
                handleHoverResult(resultValue.isObject() ? resultValue.toObject() : QJsonObject());
            } else if (method == "shutdown") {
                // сервер подтвердил готовность к завершению
                qInfo() << "LSP < Получен ответ на shutdown";
                QJsonObject exitMsg;
                exitMsg["jsonrpc"] = "2.0";
                exitMsg["method"] = "exit";
                sendMessage(exitMsg);
            } 
        } else {
            QJsonObject errorObj = message["error"].toObject();
            int code = errorObj["code"].toInt(); 
            QString errorMsg = errorObj["message"].toString();
            qWarning() << "LSP < Ошибка в ответе на запрос ID" << id << "Код:" << code << "Сообщение:" << errorMsg;
            // ----------TODO посылать сигнали клиенту что бы показать ошибка
        }
    } else if (message.contains("method")) {
        // уведомление или запрос от сервера Notification/Request
        // уведомления не содеражт айди, а запросы от сервера к клиенту - содержат
        QString method = message["method"].toString(); // имя метода, например textDocument/publishDiagnostics
        QJsonObject params; // могу отсутствовать при некоторых уведомлениях
        if (message.contains("params") && message["params"].isObject()) {
            params = message["params"].toObject();
        }
        qDebug() << "LSP < Получено уведомление или запрос от сервера, метод" << method;

        // обработчики в зависимости от метода
        if (method == "textDocument/publishDiagnostics") {
            // список ошибок и предупрждений
            handlePublishDiagnostics(params);
        } else if (method == "window/showMessage") {
            // сервер просит нас показать пользователю какое-то соо
            QJsonValue typeVal = params.value("type");
            int msgType = typeVal.isDouble() ? typeVal.toInt(4) : 4; // 1: Error,  2: Wanr,  3: Info,  4: Log
            QString text = params["message"].toString();
            if (msgType == 1) {
                qCritical() << "LSP Message (Error):" << text;
            } else if (msgType == 2) {
                qWarning() << "LSP Message (Warn):" << text;
            } else if (msgType == 3) {
                qInfo() << "LSP Message (Info):" << text;
            } else {
                qDebug() << "LSP Message (Log):" << text;
            } 
            // ------------- TODO можно показать это соо в статус баре или диалогово мокне
        } else if (method == "client/registerCapability") {
            // сервер динамически просит включить какую-то функцию, пока что ИГНОР
            qDebug() << "LSP < Получен client/registerCapability (игнорируется)";
        } else if (method == "$/progress") {
            // сервер сообщает о прогрессе долгой операции (например индексация), пока что ИГНОР
            qDebug() << "LSP < получен $/progress (игнорируется)";
        } else if (method == "window/logMessage") {
            // похоже на showMessage но для логов.
             qDebug() << "LSP Log:" << params["message"].toString();
        }
    } else {
        // тип соо не определен
        qWarning() << "LSP < Неизвесный тип сообщения:" << message;
    }
}

// !!! обработчики ответов и уведомлений !!!

// когда сервер успешно ответил на запрос инициализации
void LspManager::handleInitializeResult(const QJsonObject& result) 
{
    qInfo() << "LSP < Получен ответ на Initialize";
    // ---------- TODO добавить вывод возможностей сервера, они содержатся в capabilities, чтобы в дальнейшем знать, какие запросы можно отправлять
    // отправляем уведомление 'initialized', чтобы сервер понял, что клиент получил иформацию и он готов к работе
    QJsonObject initializedParams;
    QJsonObject initializedMsg;
    initializedMsg["jsonrpc"] = "2.0";
    initializedMsg["method"] = "initialized";
    initializedMsg["params"] = initializedParams;
    sendMessage(initializedMsg);
    qDebug() << "LSP > Отправлено уведомление initialized";

    // теперь сервер полностью готов
    m_isServerReady = true;
    qInfo() << "LSP сервер инициализирован и готов к работе";
    emit serverReady(); // посылаем сигнал клиенту, что можно общаться
}

// когда сервер присылает уведомление 'textDocument/publishDiagnostics'
void LspManager::handlePublishDiagnostics(const QJsonObject& params) 
{
    // извлекаем URI к которому относится диагностика
    QString fileUri = params["uri"].toString();
    // ЛОГ: посмотрим на весь объект params, который пришел для этого URI
    qDebug() << "[LSP_MANAGER_DEBUG] Raw JSON 'params' for publishDiagnostics URI" << fileUri << ":" << QJsonDocument(params).toJson(QJsonDocument::Compact);
    // проврека что даигностикс - массив
    QJsonValue diagnosticsValue = params.value("diagnostics");
    // ЛОГ: проверим, является ли поле 'diagnostics' массивом
    qDebug() << "[LSP_MANAGER_DEBUG] Is 'diagnostics' an array?" << diagnosticsValue.isArray();
    if (!diagnosticsValue.isArray()) {
        qWarning() << "LSP < Поле diagnostics не является массивом в publishDiagnostics";
        return;
    }

    // извлекаем массив джсон-объекто, которые описывают диагностии
    QJsonArray diagsArray = params["diagnostics"].toArray();
    // создаем список наших структур LspDiagnostic для хранения результата
    QList<LspDiagnostic> diagnosticsList;

    // проходимя по каждому элементу массива диагностики от сервера
    for (const QJsonValue& val : diagsArray) {
        if (!val.isObject()) {
            qWarning() << "[LSP_MANAGER_WARNING] Element in 'diagnostics' array is not an object:" << val;
            continue; //пропуска не-объекты
        }
        QJsonObject diagObj = val.toObject();
        qDebug() << "[LSP_MANAGER_DEBUG] Parsing diag object:" << QJsonDocument(diagObj).toJson(QJsonDocument::Compact);
        // проверяем наличие вложенных объектов
        QJsonValue rangeVal = diagObj.value("range");
        if (!rangeVal.isObject()) {
            qWarning() << "[LSP_MANAGER_WARNING] 'range' field is missing or not an object in diagnostic:" << QJsonDocument(diagObj).toJson(QJsonDocument::Compact);
            continue;
        }
        QJsonObject range = rangeVal.toObject(); // где находится проблема
        QJsonValue startVal = range.value("start");
        QJsonValue endVal = range.value("end");
        if (!startVal.isObject() || !endVal.isObject()) {
             qWarning() << "[LSP_MANAGER_WARNING] 'start' or 'end' field is missing or not an object in range:" << QJsonDocument(range).toJson(QJsonDocument::Compact);
            continue;
        }
        QJsonObject start = range["start"].toObject(); // начало диапозона
        QJsonObject end = range["end"].toObject(); // конец диапозона
        
        // создаем структуру LspDiagnostic и заполняем данными из джсон
        LspDiagnostic diag;
        diag.message = diagObj.value("message").toString(); // текст ошибки
        QJsonValue severityVal = diagObj.value("severity");
        diag.severity = severityVal.isDouble() ? severityVal.toInt(3) : 3; // серьезность может отсутствовать, по умолчанию Info (3)

        QJsonValue startLineVal = start.value("line");
        QJsonValue startCharVal = start.value("character");
        QJsonValue endLineVal = end.value("line");
        QJsonValue endCharVal = end.value("character");

        if(!startLineVal.isDouble() || !startCharVal.isDouble() || !endLineVal.isDouble() || !endCharVal.isDouble()) {
            qWarning() << "LSP < Некорректные координаты в диагностике";
            continue;
        }

        diag.startLine = startLineVal.toInt(); // строка начала
        diag.startChar = startCharVal.toInt(); // символ начала
        diag.endLine = endLineVal.toInt(); // строка конца
        diag.endChar = endCharVal.toInt(); // символ конца

        // добавляем заполненную карточку
        diagnosticsList.append(diag);
    }
    
    qDebug() << "LSP < получены диагностики для" << fileUri << ":" << diagnosticsList.size() << "шт";
    // посылаем клиенту, передавая uri файла и список найденных проблем
    emit diagnosticsReceived(fileUri, diagnosticsList);
}

// когда сервер отвечает на наш запрос автодополнения
void LspManager::handleCompletionResult(const QJsonValue& resultValue) {
    QList<LspCompletionItem> completionList; 
    QJsonArray itemsArray; // сюда массив подсказок из ответа сервера

    // сервер может вернуть результаты в разных форматах:
    // 1. Обхект, содержащий поле items (массив)
    // 2. Просто массив подсказок
    if (resultValue.isObject()) {
        QJsonObject resultObj = resultValue.toObject();
        // некоторые сервера созвращают CompletionList объхект
        if (resultObj.contains("items") && resultObj["items"].isArray()) {
            itemsArray = resultObj["items"].toArray();
        } else if (!resultObj.isEmpty()) {
            // возможно объект другой пришел
            qWarning() << "LSP < Неожиданный тип ответа на completion (не объект, не массив, не null):" << resultValue.type();
        }
        // если объект, но не содержит items, itemsArray остается пустым
    } else if (resultValue.isArray()) { // некоторые старые серверы могут возвращать просто массив
            itemsArray = resultValue.toArray();
        } else if (!resultValue.isNull()) { // может прийти просто пустой оюъект, если ещё нет подсказок
            qWarning() << "LSP < Неожиданный тип ответа на completion (не объект, не массив, не null):" << resultValue.type();
        }

    // проходимся по каждому элементу массива подсказок от сервера
    for (const QJsonValue& val : itemsArray) {
        QJsonObject itemsObj = val.toObject(); 
        LspCompletionItem item; // создаем структуру дял подсказки

        // заполняем поля структуры из джсон-объкт подсказки
        item.label = itemsObj["label"].toString(); // текст для списка
        QJsonValue insertTextVal = itemsObj.value("insertText");
        item.insertText = insertTextVal.isString() ? insertTextVal.toString() : item.label; // текст который фактически вставиться
        item.detail = itemsObj.value("detail").toString(); // тип

        // документация может быть строкой или объектом { kind: "markdown", value: "..." }
        QJsonValue docVal = itemsObj["documentation"];
        if (docVal.isString()) {
            item.documentation = docVal.toString();
        } else if (docVal.isObject()) {
            item.documentation = docVal.toObject().value("value").toString();
        }

        item.kind = itemsObj.value("kind").toInt(); // тип (функция, класс)

        // добавляем готовую подсказку в наш список
        completionList.append(item);
    }
    qDebug() << "LSP < Получено" << completionList.size() << "элементов автодопления";
    // посылаем сигнал клиенту со списком готовых подсказок
    emit completionReceived(completionList);
}

// когда сервер отвечает на наш запрос со всплывашкой (hover)
void LspManager::handleHoverResult(const QJsonObject& result) 
{
    LspHoverInfo info;

    // ответ на запрос может быть пустым (null/{}), если сервер не наше инфу
    if (result.isEmpty() || result.value("contents").isNull()) {
        qDebug() << "LSP < Получен пустой ответ на hover";
        emit hoverReceived(info); // чтобы ui мог скрыть старую подсказку
        return;
    }

    QJsonValue contentsVal = result["contents"];
    if (contentsVal.isString()) {
        // когда просто строка
        info.contents = contentsVal.toString();
    } else if (contentsVal.isObject() && contentsVal.toObject().contains("value")) {
        // объект MarkupContent (предпочтительный), содержит тип разметки и текст
        info.contents = contentsVal.toObject()["value"].toString();
    } else if (contentsVal.isArray()) {
        // массив строк или объектов MarkedString (старый формат)
        QStringList parts; // собираем все части в список строк
        for (const QJsonValue& partVal : contentsVal.toArray()) {
            if (partVal.isString()) {
                parts << partVal.toString();
            } else if (partVal.isObject() && partVal.toObject().contains("value")) { // может быть { language: 'cpp', value: 'int foo()' }
                parts << partVal.toObject()["value"].toString();
            }
        }
        // соединяем все части и разделяем их
        info.contents = parts.join("\n---\n");
    } else {
        qWarning() << "LSP < Неизвестный формат поля contents в ответ на hover";
        return; // не смогли разобрать и выходим
    }

    qDebug() << "LSP < Получена hover-информация";
    // отправляем клиенту сигнал с готовой подсказкой
    emit hoverReceived(info);
}

// когда сервер отвечает на переход к определению
void LspManager::handleDefinitionResult(const QJsonObject& resultOrArray) 
{
    QList<LspDefinitionLocation> locations; // список мест определения

    // парсинг одного объекта локации
    auto parseLocation = [&](const QJsonObject& locObj) -> LspDefinitionLocation {
        LspDefinitionLocation loc;
        loc.fileUri = locObj["uri"].toString(); // путь к файлу
        QJsonObject range = locObj["range"].toObject(); // диапозон в файле
        // начало диапозона - строка и символ
        loc.line = range["start"].toObject()["line"].toInt();
        loc.character = range["start"].toObject()["character"].toInt();
        return loc;
    };

    // провряем тип ответа от сервера
    QJsonValue resultValue =  QJsonValue(resultOrArray);
    if (resultValue.isNull()) {
        // определение не найдено
        qDebug() << "LSP < Получен null в ответе на definition";
    } else if (resultValue.isArray()) {
        // ответ на массив, может быть пустым {}
        for (const QJsonValue& val : resultValue.toArray()) {
            if (val.isObject()) { // каждый элемент должен быть объектом LOcation
                locations.append(parseLocation(val.toObject())); // парсим и добавляем
            }
        }
    } else if (resultValue.isObject()) {
        // ответ - один объект Location (старый формат)
        locations.append(parseLocation(resultValue.toObject()));
    } else {
        qWarning() << "LSP < Неизвсетный формат ответа на definition";
    }

    qDebug() << "LSP < Получено" << locations.size() << "мест определения";
    // посылаем клиенту список найденных мест (может быть пустым)
    emit definitionReceived(locations);
}

// !!! публичные методы для отправки запросов и уведомлений из MainWindow
void LspManager::notifyDidOpen(const QString& fileUri, const QString& text, int version)
{
    if (!m_isServerReady) return; // если сервер не готово

    // собираем джсон textDocument с информацией о файле
    QJsonObject textDocument;
    textDocument["uri"] = fileUri;
    textDocument["languageId"] = m_languageId; // язык
    textDocument["version"] = version; // версия файла, начиная с 1
    textDocument["text"] = text;
    QJsonObject params;
    params["textDocument"] = textDocument;

    // соо уведомление (без айди)
    QJsonObject message;
    message["jsonrpc"] = "2.0";
    message["method"] = "textDocument/didOpen";
    message["params"] = params;
    sendMessage(message);
    qDebug() << "LSP > Отправлено didOpen для" << fileUri;
}

// уведомляем сервак, что файл был изменен
void LspManager::notifyDidChange(const QString& fileUri, const QString& text, int version)
{
    if (!m_isServerReady) return;

    QJsonObject params;
    QJsonObject textDocument;
    textDocument["uri"] = fileUri;
    textDocument["version"] = version;
    params["textDocument"] = textDocument;

    // сообщение об измнениях, отправляем ВЕСЬ новый текст файла целиком
    // --------------TODO сделать диффиренцированное сохранения
    QJsonArray changes; // массив изменений
    QJsonObject changeEvent;
    changeEvent["text"] = text;
    changes.append(changeEvent);
    params["contentChanges"] = changes; // добавляем массив изменений в параметры

    // собираем и отправляем уведомление
    QJsonObject message;
    message["jsonrpc"] = "2.0";
    message["method"] = "textDocument/didChange";
    message["params"] = params;
    sendMessage(message);
}

// уведомление что сервак закрыл
void LspManager::notifyDidClose(const QString& fileUri)
{
    if (!m_isServerReady) return;

    QJsonObject params;
    QJsonObject textDocument;
    textDocument["uri"] = fileUri;
    params["textDocument"] = textDocument;

    QJsonObject message;
    message["jsonrpc"] = "2.0";
    message["method"] = "textDocument/didClose";
    message["params"] = params;
    sendMessage(message);
    qDebug() << "LSP > Отправлено didClose для" << fileUri;
}

// запрашивает у сервака варинаты автодопления
void LspManager::requestCompletion(const QString& fileUri, int line, int character, int triggerKind)
{
    if (!m_isServerReady) return;

    QJsonObject params;
    QJsonObject textDocument;
    textDocument["uri"] = fileUri;
    params["textDocument"] = textDocument;
    // позиция кусрора где запрошено автодопление (строка/символ)
    QJsonObject position;
    position["line"] = line;
    position["character"] = character;
    params["position"] = position;
    // контекста вызова (вручную или по символу)
    QJsonObject context;
    context["triggerKind"] = triggerKind;
    params["context"] = context;

    QJsonObject message;
    message["jsonrpc"] = "2.0";
    message["id"] = ++m_requestId;
    message["method"] = "textDocument/completion";
    message["params"] = params;
    sendMessage(message);
    qDebug() << "LSP > Запрошено автодополнения для" << fileUri << "в" << line << ":" << character << "ID:" << m_requestId;
    // -------- TODO дописать, чтобы принимался конкретный айдишник процесс для дальнейшего распознавания ответа
}

// запрос всплывашки (подсказки-hover)
void LspManager::requestHover(const QString& fileUri, int line, int character)
{
    if (!m_isServerReady) return;

    QJsonObject params;
    QJsonObject textDocument;
    textDocument["uri"] = fileUri;
    params["textDocument"] = textDocument;
    QJsonObject position;
    position["line"] = line;
    position["character"] = character;
    params["position"] = position;

    QJsonObject message;
    message["jsonrpc"] = "2.0";
    message["id"] = ++m_requestId;
    message["method"] = "textDocument/hover";
    message["params"] = params;
    sendMessage(message);
    qDebug() << "LSP > Запрошен hover для" << fileUri << "в" << line << ":" << character << "ID:" << m_requestId;
    // --------- TODO сохранить ID и имя метода
}

// запрос место определения символа
void LspManager::requestDefinition(const QString& fileUri, int line, int character)
{
    if (!m_isServerReady) return;

    QJsonObject params;
    QJsonObject textDocument;
    textDocument["uri"] = fileUri;
    params["textDocument"] = textDocument;
    QJsonObject position;
    position["line"] = line;
    position["character"] = character;
    params["position"] = position;

    QJsonObject message;
    message["jsonrpc"] = "2.0";
    message["id"] = ++m_requestId;
    message["method"] = "textDocument/definition";
    message["params"] = params;
    sendMessage(message);
    qDebug() << "LSP > Запрошен definition для" << fileUri << "в" << line << ":" << character << "ID:" << m_requestId;
    // --------- TODO сохранить ID и имя метода
}

// !!! конвектор позиций из QPlainTextEdit (одно число - номер символа) в LSP-формат (два числа - номер строки и номер символа в строке)
QPoint LspManager::editorPosToLspPos(QTextDocument *doc, int editorPos)
{
    if (!doc || editorPos < 0) {
        return QPoint(-1, -1); // некорректный ввод
    }

    // находим блок текста (Строку), которая содержит editorPos
    QTextBlock tb = doc->findBlock(editorPos);
    if (!tb.isValid()) {
        // если позиция за пределами документа, то вернем позицию конца последней строки
        tb = doc->lastBlock();
        if (!tb.isValid()) {
            return QPoint(0, 0); // пустой документ?
        } 
        return QPoint(doc->blockCount() - 1, tb.length() - 1); // конец документа (-1, т.к нет \n)
        // return QPoint(-1, -1); // можно вернуть ошибку
    }

    int line = tb.blockNumber(); // номер строки
    int character = editorPos - tb.position(); // позиция символа нутри строки
    // // проходимся посимвольно по тексту до нужной позиции
    // for (int i = 0; i < text.length() && currentPos < editorPos; ++i) {
    //     if (text[i] == '\n') { // если встретился перевод строки
    //         line++;
    //         character = 0;
    //     } else {
    //         // -------- TODO усложнить и уточнить логику, потмоу что табуляция занимает больше 1 места, так же многобайтные символа в utf-8 могут занимат больше 1 места и быть одним символом
    //         character++;
    //     }
    //     currentPos++;
    // }
    // // если позиция указывает на '\n'
    // if (currentPos == editorPos && editorPos > 0 && text.at(editorPos-1) == QChar('\n')) {
    //     // мы стоим на символе перевода строки, значит это позиция 0 следующей строки
    //     character = 0;
    // }

    // ------- TODO табы пока что не учитываются
    return QPoint(line, character);
}

// обратный конвектор из лсп
int LspManager::lspPosToEditorPos(QTextDocument *doc, int line, int character)
{
    if (!doc || line < 0 || character < 0) {
        return -1; // некорректный ввод
    }

    QTextBlock tb = doc->findBlockByNumber(line);
    if (!tb.isValid()) {
        // запрашиваем строка не существует (может слишком большая)
        // можно вернуть конец документа или -1
        // return doc->characterCount() - 1; // позиция последнего символа
        return -1;
    }
    int lineStartPosition = tb.position(); // начальная позиция строки в документе
    int posInLine = character; // символ внутри строки

    // ------- TODO табы и сложные символы обрабатывать, потому character - смещение от начала строки, а он может быть больше длины строки и пока что возвращаем символ конца строки
    if (posInLine >= tb.length()) {
        return qMin(lineStartPosition + posInLine, doc->characterCount() - 1);
    }
    
    // <<< ЛОГ ВНУТРИ КОНВЕРТЕРА >>>
    QString blockText = tb.text(); // Получим текст строки для лога
    qDebug() << "[lspPosToEditorPos] Input line:" << line << "char:" << character
             << " -> Found block:" << tb.blockNumber() << "startPos:" << lineStartPosition
             << "length:" << tb.length() << "text: '" << blockText.left(20) << "...'" // Показать начало текста
             << " -> Calculated EditorPos:" << (lineStartPosition + character);
    // <<< КОНЕЦ ЛОГА >>>

    return lineStartPosition + posInLine;
    // for (int pos = 0; pos < text.length(); ++pos) {
    //     // если мы на нужной строке и на нужном символе
    //     if (currentLine == line && currentCharacter == character) {
    //         return pos;
    //     }

    //     if (text[pos] == '\n') {
    //         currentLine++;
    //         currentCharacter = 0;
    //     } else {
    //         currentCharacter++;
    //         // ----- TODO tabs and utf-8
    //     }
    // }

    // // если весь текст пройден и искомая позиция совпадает с концом файла
    // if (currentLine == line && currentCharacter == character) {
    //     return text.length();
    // }

    // // например запросили строку 100 в файла с 50 строками
    // return -1;
}

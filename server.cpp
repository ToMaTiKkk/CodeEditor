#include "server.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QColor>
#include <QRandomGenerator>
#include <QJsonArray>

Server::Server(quint16 port, QObject *parent) : QObject(parent)
{
    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("Collab Editor Server"), QWebSocketServer::NonSecureMode, this); // создание сервера Collab Editor Server в режиме SSL
    if(m_pWebSocketServer->listen(QHostAddress::Any, port)) // принимает подключения с любого интерфейса, то есть и локально и по сети
    {
        qDebug() << "Сервер слушает порт:" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection, this, &Server::onNewConnection);
        connect(m_pWebSocketServer, &QWebSocketServer::closed, this, &Server::onClosed);

        availableColors << QColor("#D81B60") << QColor("#8E24AA") << QColor("#3949AB") << QColor("#00897B") << QColor("#F4511E") << QColor("#FDD835");
    }
    else
    {
        qDebug() << "Не удалось запустить сервер:" << m_pWebSocketServer->errorString();
    }
}

Server::~Server()
{
    m_pWebSocketServer->close();
    qDeleteAll(sessions);
    sessions.clear();
}

QString Server::generateShortSessionId() const
{
    const int idLength = 8;
    const QString allowedChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    QString sessionId;

    for (int i = 0; i < idLength; ++i) {
        int randomIndex = QRandomGenerator::global()->bounded(allowedChars.length());
        sessionId += allowedChars[randomIndex];
    }

    while (sessions.contains(sessionId)) {
        sessionId.clear();
        for (int i = 0; i < idLength; ++i) {
            int randomIndex = QRandomGenerator::global()->bounded(allowedChars.length());
            sessionId += allowedChars[randomIndex];
        }
    }

    return sessionId;
}

QColor Server::getNextAvailableColor()
{
    QColor color = availableColors[nextColorIndex];
    nextColorIndex = (nextColorIndex + 1) % availableColors.size();
    return color;
}

void Server::sendToSessionClients(const QString &sessionId, const QString &message, QWebSocket *excludeSocket)
{
    if (!sessions.contains(sessionId)) {
        qDebug() << "Error: Session not found:" << sessionId;
        return;
    }

    Session* session = sessions[sessionId];
    for (QWebSocket* client : session->clients.values()) {
        if (client != excludeSocket) {
            client->sendTextMessage(message);
        }
    }
}

void Server::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection(); // получение нового соединения
    qDebug() << "Подкл.чился новый клиент:" << pSocket;
    connect(pSocket, &QWebSocket::textMessageReceived, this, &Server::processTextMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &Server::socketDisconnected);
   // m_clients << pSocket; // добавление соединения в список всех подключений
}

void Server::createSession(const QString& clientId, const QString& username)
{
    QString sessionId = generateShortSessionId();
    sessions[sessionId] = new Session(sessionId);
    qDebug() << "Создана новая сессия:" << sessionId << " пользователем:" << username;
    joinSession(clientId, username, sessionId);
}

void Server::joinSession(const QString& clientId, const QString& username, const QString& sessionId)
{
    QWebSocket* clientSocket = socketToClientId.key(clientId);

    if (!sessions.contains(sessionId)) {
        qDebug() << "Сессия с таким индентификатором не найдена" << sessionId;
        QJsonObject error;
        error["type"] = "error";
        error["message"] = "Сессия не найдена";
        QJsonDocument errorDoc(error);
        if (clientSocket) {
            clientSocket->sendTextMessage(QString::fromUtf8(errorDoc.toJson(QJsonDocument::Compact)));
        }
        return;
    }

    Session* session = sessions[sessionId];
    if (!clientSocket) {
        qDebug() << "onJoinSession: сокет не найден" << clientId;
        return;
    }
    session->clients[clientId] = clientSocket;
    session->usernames[clientId] = username;
    clientToSession[clientId] = sessionId;
    QColor cursorColor = getNextAvailableColor();
    session->cursorColors[clientId] = cursorColor;

    qDebug() << "Клиент" << username << " присоединился к сессии" << sessionId;
    sendSessionInfo(clientId, sessionId);
    sendUserListUpdate(sessionId);
}

void Server::leaveSession(const QString& clientId)
{
    if (!clientToSession.contains(clientId)) {
        qDebug() << "Клиент не в сессии:" << clientId;
        return;
    }

    QString sessionId = clientToSession[clientId];
    Session* session = sessions.value(sessionId);
    QString username = session->usernames[clientId];

    if (session) {
        session->clients.remove(clientId);
        session->cursorPositions.remove(clientId);
        session->cursorColors.remove(clientId);
        session->usernames.remove(clientId);
        clientToSession.remove(clientId);

        qDebug() << "Клиент" << clientId << username << "покинул сессию" << sessionId;
        QJsonObject disconnectMessage;
        disconnectMessage["type"] = "user_disconnected";
        disconnectMessage["client_id"] = clientId;
        disconnectMessage["username"] = username;
        QJsonDocument doc(disconnectMessage);
        sendToSessionClients(sessionId, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        sendUserListUpdate(sessionId);

        if (session->clients.isEmpty()) {
            sessions.remove(sessionId);
            delete session;
            qDebug() << "Сессия" << sessionId << "удалена (нет клиентов)";
        }
    }
}

void Server::sendSessionInfo(const QString& clientId, const QString& sessionId) {
    if (!sessions.contains(sessionId)) {
        qDebug() << "Error: Session not found" << sessionId;
        return;
    }

    QWebSocket* clientSocket = socketToClientId.key(clientId);
    if (!clientSocket) {
        qDebug() << "Сокет не найден для:" << clientId;
        return;
    }

    Session* session = sessions[sessionId];
    QJsonObject sessionData;
    sessionData["type"] = "session_info";
    sessionData["session_id"] = sessionId;
    sessionData["text"] = session->text;

    // Отправляем историю чата
    QJsonArray chatHistory;
    for (const QJsonObject& chatMessage : session->chatMessages) {
        chatHistory.append(chatMessage);
    }
    sessionData["chat_history"] = chatHistory;

    QJsonObject cursors;
    for (const QString& otherClientId : session->cursorPositions.keys()) {
        if (otherClientId != clientId) {
            QJsonObject cursorInfo;
            cursorInfo["username"] = session->usernames.value(otherClientId);
            cursorInfo["position"] = session->cursorPositions[otherClientId];
            cursorInfo["color"] = session->cursorColors[otherClientId].name();
            cursors[otherClientId] = cursorInfo;
        }
    }
    sessionData["cursors"] = cursors;
    QJsonDocument sessionDataDoc(sessionData);
    clientSocket->sendTextMessage(QString::fromUtf8(sessionDataDoc.toJson(QJsonDocument::Compact)));
}

void Server::sendUserListUpdate(const QString& sessionId)
{
    if (!sessions.contains(sessionId)) {
        qDebug() << "Сессия не найдена: при отправке списка пользователей" << sessionId;
        return;
    }

    Session* session = sessions[sessionId];
    QJsonObject userList;
    userList["type"] = "user_list_update";
    QJsonArray users;
    for (const QString& clientId : session->usernames.keys()) {
        QJsonObject user;
        user["client_id"] = clientId;
        user["username"] = session->usernames[clientId];
        user["color"] = session->cursorColors[clientId].name();
        users.append(user);
    }
    userList["users"] = users;
    QJsonDocument doc(userList);
    sendToSessionClients(sessionId, QString::fromUtf8(doc.toJson()), nullptr);

}

void Server::processTextMessage(const QString &message) {
    QWebSocket *senderSocket = qobject_cast<QWebSocket*>(sender());
    if (!senderSocket) return;

    qDebug() << "Получено сообщение от клиента:" << message;

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();
    QString clientId = obj["client_id"].toString();

    if (type == "chat_message") {
        QString sessionId = clientToSession.value(clientId);
        if (sessionId.isEmpty()) {
            qDebug() << "Ошибка, клиент" << clientId << "не находится в сессии";
            return;
        }

        Session *session = sessions[sessionId];
        QString username = session->usernames.value(clientId);
        QString chatMessage = obj["message"].toString();

        // Создаём объект сообщения чата
        QJsonObject chatMessageObj;
        chatMessageObj["type"] = "chat_message";
        chatMessageObj["username"] = username;
        chatMessageObj["message"] = chatMessage;
        chatMessageObj["color"] = session->cursorColors[clientId].name();

        // Отправляем сообщение всем клиентам в сессии
        QJsonDocument chatDoc(chatMessageObj);
        QString jsonMessage = QString::fromUtf8(chatDoc.toJson(QJsonDocument::Compact));
        qDebug() << "Отправлено сообщение всем клиентам:" << jsonMessage;
        sendToSessionClients(sessionId, jsonMessage);
    }
}


void Server::socketDisconnected()
{
    QWebSocket *senderSocket = qobject_cast<QWebSocket*>(sender()); // определяем отключился ли клиент

    if (!senderSocket) return;

    QString clientId = socketToClientId[senderSocket];
    if (!clientId.isEmpty())
    {
        leaveSession(clientId);
    }
    socketToClientId.remove(senderSocket);
    senderSocket->deleteLater();
}

void Server::onClosed()
{
    qDebug() << "Сервер закрыт";
}

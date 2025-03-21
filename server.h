#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QDebug>
#include <QMap>
#include <QColor>

struct Session {
    QString sessionId;
    QString text;
    QMap<QString, QWebSocket*> clients;
    QMap<QString, int> cursorPositions;
    QMap<QString, QColor> cursorColors;
    QMap<QString, QString> usernames;
    QList<QJsonObject> chatMessages; // Новое поле для хранения сообщений чата

    Session(const QString& id) : sessionId(id) {}
};

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(quint16 port, QObject *parent = nullptr);
    ~Server();

private slots:
    void onNewConnection();
    void processTextMessage(const QString &message);
    void socketDisconnected();
    void onClosed();

    void sendToSessionClients(const QString& sessionId, const QString& message, QWebSocket* excludeClient = nullptr);
    void createSession(const QString& clientId, const QString& username);
    void joinSession(const QString& clientId, const QString& username, const QString& sessionId);
    void leaveSession(const QString& clientId);
    void sendUserListUpdate(const QString &sessionId);
    void sendSessionInfo(const QString& clientId, const QString& sessionId);

private:
    QWebSocketServer *m_pWebSocketServer; // указатель на websocket-соединения
    //QList<QWebSocket *> m_clients; // список всех подключенных клиентов
    // QHash<QWebSocket*, QString> m_userNames;
    // QMap<QWebSocket*, QString> clientId;
    QString generateShortSessionId() const;
    QMap<QString, Session*> sessions;
    QMap<QWebSocket*, QString> socketToClientId;
    QMap<QString, QString> clientToSession;

    QList<QColor> availableColors;
    int nextColorIndex = 0;
    QColor getNextAvailableColor();

};

#endif // SERVER_H

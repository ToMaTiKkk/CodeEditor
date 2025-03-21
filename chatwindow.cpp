#include "chatwindow.h"
#include <QDebug>

ChatWindow::ChatWindow(const QString &clientId, const QString &username, QWidget *parent)
    : QDialog(parent), clientId(clientId), username(username) {
    setupUI(); // Настройка интерфейса

    // Инициализация WebSocket
    webSocket = new QWebSocket();
    connect(webSocket, &QWebSocket::connected, this, &ChatWindow::onConnected);
    connect(webSocket, &QWebSocket::textMessageReceived, this, &ChatWindow::onTextMessageReceived);
    connect(webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &ChatWindow::onSocketError);

    // Подключение к серверу
    webSocket->open(QUrl("ws://YOUR_WEBSOCKET_HOST:YOUR_WEBSOCKET_PORT")); // Замените на адрес вашего сервера
}

ChatWindow::~ChatWindow() {
    webSocket->close();
    webSocket->deleteLater();
}

void ChatWindow::setupUI() {
    // Настройка интерфейса
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Поле для отображения сообщений
    chatTextEdit = new QTextEdit(this);
    chatTextEdit->setReadOnly(true); // Только для чтения
    layout->addWidget(chatTextEdit);

    // Поле для ввода сообщений
    messageLineEdit = new QLineEdit(this);
    layout->addWidget(messageLineEdit);

    // Кнопка отправки сообщения
    sendButton = new QPushButton("Отправить", this);
    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::onSendButtonClicked);
    layout->addWidget(sendButton);

    setLayout(layout);
    setWindowTitle("Чат - " + username);
    resize(400, 300);
}

void ChatWindow::onConnected() {
    qDebug() << "Подключено к серверу";
    // Отправляем серверу информацию о подключении
    QJsonObject joinMessage;
    joinMessage["type"] = "join_session";
    joinMessage["client_id"] = clientId;
    joinMessage["username"] = username;
    joinMessage["session_id"] = "default_session"; // Замените на реальный ID сессии
    QJsonDocument doc(joinMessage);
    webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void ChatWindow::onTextMessageReceived(const QString &message) {
    qDebug() << "Получено сообщение от сервера:" << message;

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "chat_message") {
        // Отображаем сообщение в чате
        QString username = obj["username"].toString();
        QString messageText = obj["message"].toString();
        QString color = obj["color"].toString();

        qDebug() << "Отображение сообщения:" << username << ":" << messageText;

        chatTextEdit->append(QString("<font color='%1'><b>%2:</b></font> %3")
                                 .arg(color)
                                 .arg(username)
                                 .arg(messageText));
    }
}

void ChatWindow::onSendButtonClicked() {
    QString message = messageLineEdit->text();
    if (!message.isEmpty()) {
        sendChatMessage(message);
        messageLineEdit->clear();
    }
}

void ChatWindow::sendChatMessage(const QString &message) {
    QJsonObject chatMessage;
    chatMessage["type"] = "chat_message";
    chatMessage["client_id"] = clientId;
    chatMessage["message"] = message;
    QJsonDocument doc(chatMessage);
    webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void ChatWindow::onSocketError(QAbstractSocket::SocketError error) {
    qDebug() << "Ошибка WebSocket:" << error;
    chatTextEdit->append("Ошибка подключения к серверу.");
}

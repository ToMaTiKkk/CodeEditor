#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QDialog>
#include <QWebSocket>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>


class ChatWindow : public QDialog {
    Q_OBJECT

public:
    explicit ChatWindow(const QString &clientId, const QString &username, QWidget *parent = nullptr);
    ~ChatWindow();

private slots:
    void onConnected(); // Слот для обработки успешного подключения к серверу
    void onTextMessageReceived(const QString &message); // Слот для обработки входящих сообщений
    void onSendButtonClicked(); // Слот для отправки сообщения
    void onSocketError(QAbstractSocket::SocketError error); // Слот для обработки ошибок сокета

private:
    QWebSocket *webSocket; // WebSocket для подключения к серверу
    QString clientId; // Идентификатор клиента
    QString username; // Имя пользователя

    // Элементы интерфейса
    QTextEdit *chatTextEdit; // Поле для отображения сообщений
    QLineEdit *messageLineEdit; // Поле для ввода сообщений
    QPushButton *sendButton; // Кнопка отправки сообщения

    void setupUI(); // Настройка интерфейса
    void sendChatMessage(const QString &message); // Отправка сообщения на сервер
};

#endif // CHATWINDOW_H

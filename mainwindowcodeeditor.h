#ifndef MAINWINDOWCODEEDITOR_H
#define MAINWINDOWCODEEDITOR_H

#include "cursorwidget.h"
#include "linehighlightwidget.h"
#include "cpphighlighter.h"
#include <QMainWindow>
#include <QFileSystemModel>
#include <QtWebSockets/QWebSocket>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUuid>
#include <QMap>
#include <QScrollBar>
#include <QCheckBox>
#include <QIcon>
#include <QCursor>
#include <QTextEdit>
#include <QLineEdit>
#include <QToolButton>
#include <QSplitter>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindowCodeEditor;
}
QT_END_NAMESPACE

class MainWindowCodeEditor : public QMainWindow
{
    Q_OBJECT

public:
    MainWindowCodeEditor(QWidget *parent = nullptr);
    ~MainWindowCodeEditor();

private slots: // функции, которые будут вызваны в ответ на определенные события
    bool eventFilter(QObject *obj, QEvent *event) override;

    void onOpenFileClicked(); // вызывается при вызове пункта мен дл открытия файлы
    void onSaveFileClicked(); // сохранение файла
    void onSaveAsFileClicked(); // сохрание под новым именем
    void onExitClicked(); // для выхода из приложения
    void onNewFileClicked(); // для создания нового файла (простое очищение редактора, файл надо будет сохранять вручную)
    void onOpenFolderClicked(); // для открытия папки и отображения ее в дереве
    //void toggleTheme(); //смена темы
    void onFileSystemTreeViewDoubleClicked(const QModelIndex &index); // когда пользователь дважды кликает по файлу в дереве, то оно открывается в редакторе

    void onContentsChange(int position, int charsRemoved, int charsAdded);
    void onTextMessageReceived(const QString &message);
    void onDisconnected();
    void onConnected();

    void onCursorPositionChanged();

    void onVerticalScrollBarValueChanged(int value);
    void updateRemoteWidgetGeometry(QWidget* widget, int position);
    void updateLineHighlight(const QString& senderId, int position);

    void onToolButtonClicked();
    void applyCurrentTheme();

    void onCreateSession();
    void onJoinSession();
    void onShowUserList();
    void onLeaveSession();
    bool confirmChangeSession(const QString& message);
    void clearRemoteInfo();

    void connectToServer(); // функция для подключения или переподключения
    void disconnectFromServer(); // функция для отключения
    void updateUserListUI(); // обновление списка пользователей в интерфейсе
    // void onUserMenuItemClicked(QAction* action);

    void onMutedStatusUpdate(const QString& clientId, bool isMuted);
    // bool canEdit();
    void updateMutedStatus();
    void onAdminChanged(const QString& newAdminId);

    void onMuteUnmute(const QString targetClientId);
    void onTransferAdmin(const QString targetClientId);
    void showUserInfo(const QString targetClientId);
    // void updateMutedStatus();

    // чатик
    void toggleChat(); // Переключение видимости чата
    void sendMessage(); // Отправка сообщения
    //void onTextMessagesReceived(const QString &message);
    void handleIncomingMessage(const QJsonObject &json);
    void on_toolButton_clicked();

private:
    Ui::MainWindowCodeEditor *ui; // доступ к элементами интерфейса .ui
    QString currentFilePath; // хранение пути к текущему открытому файлу, используется, чтобы знать куда записывать изменения
    QFileSystemModel *fileSystemModel; // добавление указателя на QFileSystemmodel (древовидный вид файловый системы слева)
    QWebSocket *socket = nullptr;
    CppHighlighter *highlighter;
    bool loadingFile = false;
    bool m_isDarkTheme;
    bool m_isAdmin;
    QMenu *m_userListMenu; // добавление для списка пользователей
    QAction *m_currentUserAction; // текущий выбранный пункт меню пользователя (для контекстного меню списка пользователей в сессии)
    QAction *m_muteUnmuteAction;
    QAction *m_transferAdminAction;
    QAction *m_infoAction;
    QCheckBox* m_themeCheckBox;
    QMap<QString, int> m_mutedClients;
    QMap<QString, CursorWidget*> remoteCursors; // словарь с курсора клиентов, ключ - айди, значение - виджет курсора
    QMap<QString, LineHighlightWidget*> remoteLineHighlights; // хранение подсветки строки, где курсор пользователя, uuid - подсветка
    QString m_username;
    QString m_clientId; // хранение уникального идентификатора клиента, пересоздается при каждом запуске программы
    QString m_sessionId;
    QList<QJsonObject> cursorUpdates; // хранение последних обновлений позиций курсора
    QMap<QString, int> lastCursorPositions; // хранение позиций всех курсоров других пользователей
    QMap<QString, QJsonObject> remoteUsers; // client_id -> {username, color}
    QWidget *chatWidget; // Виджет чата
    QTextEdit *chatDisplay; // Поле для отображения сообщений
    QLineEdit *chatInput; // Поле для ввода сообщений

    //новое разделение окон
    bool isChatVisible = false;    // Флаг видимости чата
};
#endif // MAINWINDOWCODEEDITOR_H

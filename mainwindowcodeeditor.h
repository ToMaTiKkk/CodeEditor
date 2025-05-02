#ifndef MAINWINDOWCODEEDITOR_H
#define MAINWINDOWCODEEDITOR_H

#include "terminalwidget.h"
#include "cursorwidget.h"
#include "linehighlightwidget.h"
#include "cpphighlighter.h"
#include "linenumberarea.h"
#include "lspmanager.h"
#include "completionwidget.h"
#include "diagnostictooltip.h"
#include "codeplaintextedit.h"
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
#include <QPushButton>
#include <QSplitter>
#include <QKeyEvent>
#include <QTextDocumentFragment>
#include <QMessageBox>
#include <QClipboard>
#include <QTime>
#include <QScrollArea>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QFontMetrics>
#include <QSystemTrayIcon>
#include <QAction>

#include<QList>
#include <QPlainTextEdit>

// #include <QDockWidget>
// #include <QListWidget>

// расширение->язык
const QMap<QString, QString> g_extensionToLanguage = {         // первое расширение файла, второе languageId, который ждет лсп сервер по типу (clangd, pylsp....)
                                                {"cpp", "cpp"},
                                                {"cc", "cpp"},
                                                {"c", "cpp"},
                                                {"h", "cpp"},
                                                {"py", "python"},
                                                {"js", "typescript"},
                                                {"ts", "typescript"},
                                                {"java", "java"},
                                                {"go", "go"},
                                                };
// язык->имя сервера LSP
const QMap<QString, QString> g_defaultLspExecutables = {
    {"cpp",        "clangd"},
    {"python",     "pyright"},
    {"typescript", "typescript-language-server"},
    {"java",       "jdtls"},
    {"go",         "gopls"},
};

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindowCodeEditor;
class QAction;
}
QT_END_NAMESPACE

class MainWindowCodeEditor : public QMainWindow
{
    Q_OBJECT

public:
    MainWindowCodeEditor(QWidget *parent = nullptr);
    ~MainWindowCodeEditor();

private slots: // функции, которые будут вызваны в ответ на определенные события

    void onOpenFileClicked(); // вызывается при вызове пункта мен дл открытия файлы
    void onSaveFileClicked(); // сохранение файла
    void onSaveAsFileClicked(); // сохрание под новым именем
    void onExitClicked(); // для выхода из приложения
    void onNewFileClicked(); // для создания нового файла (простое очищение редактора, файл надо будет сохранять вручную)
    void onOpenFolderClicked(); // для открытия папки и отображения ее в дереве
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
    void onSaveSessionClicked();
    void onCopyIdClicked();
    void onCreateSession();
    void onJoinSession();
    void onShowUserList();
    void onLeaveSession();
    bool confirmChangeSession(const QString& message);
    void clearRemoteInfo();

    void connectToServer(); // функция для подключения или переподключения
    void disconnectFromServer(); // функция для отключения
    void updateUserListUI(); // обновление списка пользователей в интерфейсе

    void onMutedStatusUpdate(const QString& clientId, bool isMuted);
    void updateMutedStatus();
    void updateMuteTimeDisplay(const QString& clientId);
    void updateStatusBarMuteTime();
    void updateUserListUser(const QString& clientId);
    //void updateAllUsersMuteTimeDisplay();
    //void updateUserInfoMuteTime();
    void updateMuteTimeDisplayInUserInfo();
    void stopMuteTimer();
    QString formatMuteTime(const QString& clientId);
    void onAdminChanged(const QString& newAdminId);

    void onMuteUnmute(const QString targetClientId);
    void onTransferAdmin(const QString targetClientId);
    void showUserInfo(const QString targetClientId);

    // чатик
    //void toggleChat(); // Переключение видимости чата
    void sendMessage(); // Отправка сообщения
    //void onTextMessagesReceived(const QString &message);
    //void handleIncomingMessage(const QJsonObject &json);
    //void on_toolButton_clicked();
    //void keyPressEvent(QKeyEvent *event) override;
    void scrollToBottom(); // Новый слот для прокрутки

    void on_actionChangeTheme_triggered();
    void updateChatButtonIcon();
    void closeEvent(QCloseEvent *event) override;

    void on_actionToDoList_triggered();

    void on_actionTerminal_triggered(); // слот для терминала

    // слоты для обработки сигналов от LspManager
    void onLspServerReady();
    void onLspServerStopped();
    void onLspServerError(const QString& message);
    void onLspDiagnosticsReceived(const QString& fileUri, const QList<LspDiagnostic>& diagnostics);
    void onLspCompletionReceived(const QList<LspCompletionItem>& items);
    void onLspHoverReceived(const LspHoverInfo& hoverInfo);
    void onLspDefinitionReceived(const QList<LspDefinitionLocation>& locations);
    // слот для обработки выбора в виджете автодополнения
    void applyCompletion(const QString& textToInsert);
    // слот дял таймера запроса hover
    void showDiagnoticTooltipOrRequestHover();
    //void onDiagnosticItemActivated(QListWidgetItem* item);
    void nextDiagnostic();
    void prevDiagnostic();

    void showFindPanel(); // по сути она еще и закрывает, не хочу делать отдельную фукнцию)))
    void findNext();
    void findPrevious();
    void updateFindHighlights(); //.для подсветки всех найденных слов


    void on_actionFindPanel_triggered();

private:
    Ui::MainWindowCodeEditor *ui; // доступ к элементами интерфейса .ui

    // настройки
    void setupMainWindow();       // основные настройки окна
    void setupCodeEditorArea();   // редактора и нумерации
    void setupChatWidget();       // чата
    void setupUserFeatures();     // меню пользователей, таймера и тп
    void setupMenuBarActions();   // подключение сигналов меню
    void setupFileSystemView();   // дерева файлов
    void setupNetwork();          // WebSocket, client_id
    void setupThemeAndNick();     // тема и никнейм
    void setupTerminalArea();     // терминал
    void compileAndRun();         // автозапуск кода

    // LSP
    void extracted(QString &languageId, QString &lspExecutable);
    void setupLsp(); // найстрока и запуска
    void setupLspCompletionAndHover();
    void triggerCompletionRequest(); // инициировать запрос автодополнения
    void triggerDefinitionRequest(); // инициировать запрос определения
    void updateDiagnosticsView(); // обновить подчеркивания ошибок в редакторе
    QString getFileUri(const QString& localPath) const; // конвектировать локальный путь в URI
    QString getLocalPath(const QString& fileUri) const; // обратный конвектор
    QString getPrefixBeforeCursor(const QTextCursor& cursor);
    void createAndStartLsp(const QString& languageId);
    void onLspSettings();
    bool ensureLspForLanguage(const QString& languageId);
    void updateLspStatus(const QString& text);

    // переопределение событий для hover и хоткеев
    bool eventFilter(QObject *obj, QEvent *event) override;
    void handleEditorKeyPressEvent(QKeyEvent *event); // обработчик нажатий в редакторе
    void handleEditorMouseMoveEvent(QMouseEvent *event); // обработчик движения мыши в редакторе
    // void handleEditorMousePressEvent(QMouseEvent *event); // для Ctrl+Click

    QString currentFilePath; // хранение пути к текущему открытому файлу, используется, чтобы знать куда записывать изменения

    QFileSystemModel *fileSystemModel; // добавление указателя на QFileSystemmodel (древовидный вид файловый системы слева)
    QWebSocket *socket = nullptr;
    CppHighlighter *highlighter;
    bool loadingFile = false;
    bool m_isDarkTheme;
    bool m_isAdmin;
    LineNumberArea *lineNumberArea;
    CodePlainTextEdit *m_codeEditor;
    bool maybeSave();
    QMenu *m_userListMenu; // добавление для списка пользователей
    QAction *m_currentUserAction; // текущий выбранный пункт меню пользователя (для контекстного меню списка пользователей в сессии)
    QAction *m_muteUnmuteAction;
    QAction *m_transferAdminAction;
    QAction *m_infoAction;
    QCheckBox* m_themeCheckBox;
    QString m_currentUserInfoClientId;
    QMessageBox *m_userInfoMessageBox;
    QTimer *m_muteTimer; // таймер для обновления времени мута
    QLabel *m_muteTimeLabel; // для отображения времени мута (в списке пользователей)
    QHash<QString, qint64> m_muteEndTimes; // словарь для хранения времени мута каждого клиента
    QString m_currentMessageBoxClientId;
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
    // QTextEdit *chatDisplay; // Поле для отображения сообщений
    QLineEdit *chatInput; // Поле для ввода сообщений

    // чат
    QScrollArea *chatScrollArea;     // Вместо chatDisplay
    QWidget *messageListWidget;    // Контейнер внутри ScrollArea
    QVBoxLayout *messagesLayout;   // Layout для контейнера
    QString m_sessionPassword;
    QByteArray pendingSessionSave; // хранение отложенного сохранения сессий
    QPushButton* m_chatButton;
    QPushButton* m_runButton;
    QSystemTrayIcon *m_trayIcon = nullptr;
    
    QToolButton *m_lspStatusLabel; // статус индикации
    QString m_currentLspLanguageId;
    bool m_shouldSaveAfterCreation = false;
    LspManager *m_lspManager = nullptr; // Lsp-менеджер
    CompletionWidget *m_completionWidget = nullptr; // виджет автодоплнения
    QTimer *m_hoverTimer = nullptr; // таймер для отложенного запроса hover
    QPoint m_lastMousePosForHover; // последняя позиция мыши для hover (всплывашка)
    DiagnosticTooltip* m_diagnosticTooltip; // кастомный тултип
    bool m_isDiagnosticTooltipVisible;
    QPair<int, int> m_currentlyShownTooltipPange; // startPos and endPos, храним диапозон информации, что сейчас показывает тултип
    QPoint calculateTooltipPosition(const QPoint& globalMousePos);
    QSet<QString> m_disableLanguages;
    //QDockWidget* m_diagnosticsDock;
    //QListWidget* m_diagnosticsList;
    QToolButton* m_diagnosticsStatusBtn;

    // управление версиями и состоянии LSP для открытого файла
    QString m_currentLspFileUri; // URI текущего файла
    int m_currentDocumentVersion = 0; // счетчик версий для лсп
    QString m_projectRootPath; // путь к корневой папке проекта для LSP
    QMap<QString, QList<LspDiagnostic>> m_diagnostics; // хранение диагностик по файлам

    int m_pendingSaveDays = 0;
    //новое разделение окон
    bool isChatVisible = false;    // Флаг видимости чата

    // Добавляем приватный метод для создания виджета сообщения
    void addChatMessageWidget(const QString &username, const QString &text, const QTime &time, bool isOwnMessage);

    // инициализация терминала
    TerminalWidget *m_terminalWidget = nullptr;
    bool m_isTerminalVisible = false;


    QString getCurrentWordBeforeCursor(QTextCursor cursor);

    QWidget*     m_findPanel = nullptr;
    QLineEdit*   m_findLineEdit = nullptr;
    QPushButton* m_findNextButton = nullptr;
    QPushButton* m_findPrevButton = nullptr;
    QPushButton* m_findCloseButton = nullptr;

    QList<QTextEdit::ExtraSelection> m_findSelections; //ля подсветки результатов поиска
    QTextCharFormat m_findFormat;
};
#endif // MAINWINDOWCODEEDITOR_H

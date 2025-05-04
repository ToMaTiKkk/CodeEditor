#ifndef LSPMANAGER_H
#define LSPMANAGER_H

#include <QObject>
#include <QProcess> // нужен для запуска и управления внешней программы, экспертами, такие как clangd
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <QPoint>
#include <QTextDocument>

// !!! структуры ъранения данных !!!
// описания ошибок или предупреждений в коде
struct LspDiagnostic {
    QString message; // сообещние об ошибке
    int severity; // серьезность проблемы (1 - ошибка, 2- предупреждение!
    int startLine, startChar, endLine, endChar; // где именно к оде (строке или символ начал и конца) находится проблема, координаты с нуля
};

// автодополнение
struct LspCompletionItem {
    QString label; // текст который пользователь увидит в списке подсказок, по типу calculateSum
    QString insertText; // который фактически вставится calculateSum()
    QString detail; // например тип переменной или возращаещей функции
    QString documentation; // подробно описание, например за что отвечает функция
    int kind; // тип элемента, функция, класс, переменная
};

// информация во всплывашке
struct LspHoverInfo {
    QString contents; // текст который показать
};

// место, где объявлена функция или переменная
struct LspDefinitionLocation {
    QString fileUri; // путь к файлы, где объявление, например "file:///home/user/project/myclass.h"
    int line, character; // строка и символ в файле, где начинается объявление
};

class LspManager : public QObject
{
    Q_OBJECT

// будет доступно в MainWindowCodeEditor
public:
    explicit LspManager(QString serverExecutablePath = "clangd", QObject *parent = nullptr);
    ~LspManager();

    // !!! управление сервером !!!
    // запускает сервер для указанного языка и папки проекта
    bool startServer(const QString& languageId, const QString& projectRootPath);
    void stopServer();
    bool isReady() const; // проверка, готов ли сервер к общению, успешно ли прошла инициализация

    // !!! метода для отправки соо серверу !!!
    // пользователь открыл файл и посылается данный сигнал серверу, ему передается путь файла, содермижоме и номер версии
    void notifyDidOpen(const QString& fileUri, const QString& text, int version = 1);
    // текст в файле изменился, отправляется новая версия
    void notifyDidChange(const QString& fileUri, const QString& text, int version);
    // пользователь файл закрыл
    void notifyDidClose(const QString& fileUri);
    // пользователь с помощью сочетания клавиш запросил подсказки на данной позиции (строка/символ), targetKind - причина запроса (1 - вызвано вручную, 2 - ввод символа и тд)
    void requestCompletion(const QString& fileUri, int line, int character, int triggerKind = 1);
    // пользователь навел мышку на это место "что это такое?"
    void requestHover(const QString& fileUri, int line, int character);
    // пользователь хочет перейти к определению символа в этой позиции "где он объявлен?"
    void requestDefinition(const QString& fileUri, int line, int character);
    // -------------- TODO: форматирование, поиски ссылок

    // функции для перевода координат между форматом редактор (номер символа) на формат сервера (строка, символ)
    QPoint editorPosToLspPos(QTextDocument *doc, int editorPos);
    int lspPosToEditorPos(QTextDocument *doc, int line, int character);
    
    QString executablePath() const { return m_serverExecutablePath; } // путь по которому запущено LSP-ядро

// сервер сообщает MainWindow что что-то произошло
signals:
    // сигналы состояния сервера
    void serverReady();
    void serverStopped();
    void serverError(const QString& message); // сообщение об ошибке

    // !!! результаты запроса !!!
    // список ошибок и предупреждений
    void diagnosticsReceived(const QString& fileUri, const QList<LspDiagnostic>& diagnostics);
    // список варинато автодопления
    void completionReceived(const QList<LspCompletionItem>& item);
    // информация для всплывашки
    void hoverReceived(const LspHoverInfo& hoverInfo);
    // список место где объявлен символ
    void definitionReceived(const QList<LspDefinitionLocation>& locations);

// функции автоматом будут вызываться от других обхектов от QProcess
private slots:
    // когда сервер написал в свой вывод (stdout), когда прислал ответ или уведомление
    void onReadyReadStandardOutput();
    // сервер в потом ошибок что то написал (stderr)
    void onReadyReadStandardError();
    // сервак завершился сам или по команде
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    // произошла ошибка при запуске или работе сервака
    void onProcessError(QProcess::ProcessError error);

// внутренние детали сервака, скрытые от основнго приложения
private:
    QMap<qint64, QString> m_pendingRequests; // ID запроса - имя метода 
    QProcess *m_lspProcess = nullptr; // нулевой - процесс не запущен
    QString m_serverExecutablePath; // имя или полный путь к анализатору, например для cpp clangd "/usr/bin/clangd"
    QString m_languageId; // короткое имя языка, например cpp
    QString m_rootUri; // путь к корню проекта в формате URI, "file:///home/user/my_project", нужно для контекста
    bool m_isServerReady = false;
    qint64 m_requestId = 0; // счетки для айдишников, у каждого запроса свой айди, нужен для правильной идентификации и обработки ответов от сервака, потому что он присылает айдишник
    QByteArray m_buffer; // буфер для данных с сервера, потому что данные могут приходить частями, поэтому надо их накапливать

    // !!! внутренние вспомогательные методы !!!
    // отправка JSON на сервер
    void sendMessage(const QJsonObject& message);
    // обработка куска данных от сервера
    void processIncomingData(const QByteArray& data);
    // разбор одного целого json и вызова соответствующего обработчика
    void parseMessage(const QByteArray& jsonContent);

    // !!! обраотка конкретных уведов и ответов от сервера !!!
    void handleInitializeResult(const QJsonObject& result); // когда сервер ответил на запрос "initialize"
    void handlePublishDiagnostics(const QJsonObject& params); // когда уведомление "publishDiagnostics", а именно список ошибок
    void handleCompletionResult(const QJsonValue& result); // когда ответ на зпрос автодополнения
    void handleHoverResult(const QJsonObject& result); // когда ответ на hover-информацию
    void handleDefinitionResult(const QJsonObject& result); // кога ответ на запрос перехода к определению
};

#endif

#include "mainwindowcodeeditor.h"
#include "./ui_mainwindowcodeeditor.h"
#include "cursorwidget.h"
#include "linehighlightwidget.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QWebSocket>
#include <QSignalBlocker>
#include <QCoreApplication>
#include <QInputDialog>
#include <QRandomGenerator>
#include <QPushButton>
#include <QPainter>
#include <QDateTime>
#include <QKeyEvent>


MainWindowCodeEditor::MainWindowCodeEditor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowCodeEditor)
    , m_isDarkTheme(true)
    , chatWidget(nullptr)
    , m_userInfoMessageBox(nullptr)
    , m_muteTimer(new QTimer(this))
{
    ui->setupUi(this);
    this->setWindowTitle("CoEdit");
    QFont font("Fira Code", 12);
    QApplication::setFont(font);

    // Создаем виджет чата
    chatWidget = new QWidget(ui->horizontalWidget_2);
    chatWidget->setObjectName("chatWidget");

    // Устанавливаем layout для chatWidget
    QVBoxLayout *chatLayout = new QVBoxLayout(chatWidget);
    chatLayout->setContentsMargins(10, 10, 10, 10); // Отступы
    chatLayout->setSpacing(10); // Расстояние между элементами

    // Поле для отображения сообщений
    chatDisplay = new QTextEdit(chatWidget);
    chatDisplay->setReadOnly(true); // Только для чтения
    chatDisplay->setObjectName("chatDisplay");
    chatLayout->addWidget(chatDisplay);

    // Создаем горизонтальный layout для поля ввода и кнопки
    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(10);

    // Поле для ввода сообщений
    chatInput = new QLineEdit(chatWidget);
    chatInput->setObjectName("chatInput");
    inputLayout->addWidget(chatInput, 1); // Расширяется на доступное пространство

    // Кнопка отправки сообщения
    QPushButton *sendButton = new QPushButton("Send", chatWidget);
    sendButton->setObjectName("sendButton");
    inputLayout->addWidget(sendButton, 0); // Фиксированный размер кнопки

    // Добавляем горизонтальный layout в вертикальный
    chatLayout->addLayout(inputLayout);

    // Подключаем сигнал кнопки к слоту sendMessage
    connect(sendButton, &QPushButton::clicked, this, &MainWindowCodeEditor::sendMessage);

    // При необходимости можно задать растяжку для chatDisplay
    chatLayout->setStretch(0, 1);

    ui->horizontalWidget_2->layout()->addWidget(chatWidget);
    chatWidget->hide();

    // создаем тумблер для переключения темы
    m_themeCheckBox = new QCheckBox(this);
    m_themeCheckBox->setChecked(!m_isDarkTheme); // checked - вкл
    connect(m_themeCheckBox, &QCheckBox::stateChanged, this, [this](int state) {
        m_isDarkTheme = (state == Qt::Unchecked); // Unchecked - темная, Checked - светлая
        applyCurrentTheme();
        if (m_isDarkTheme) {
            m_themeCheckBox->setToolTip(tr("Темная тема"));
        } else {
            m_themeCheckBox->setToolTip(tr("Светлая тема"));
        }
    });
    // добавляем тумблер в layout
    QLabel* themeLabel = new QLabel("Тема:", this);
    QHBoxLayout *themeLayout = new QHBoxLayout;
    themeLayout->addWidget(themeLabel);
    themeLayout->addWidget(m_themeCheckBox);
    QWidget *themeWidget = new QWidget(this);
    themeWidget->setLayout(themeLayout);
    ui->menubar->setCornerWidget(themeWidget, Qt::TopRightCorner); // добавляем в правый верхний угол

    // создаем меню для списка пользователей
    m_userListMenu = new QMenu(this);
    ui->actionShowListUsers->setMenu(m_userListMenu);

    //m_muteTimer = new QTimer(this);
    connect(m_muteTimer, &QTimer::timeout, this, &MainWindowCodeEditor::updateStatusBarMuteTime); // вызывает каждую секунду, когда таймер запущен, чтоы обновлять время мьюта в статус-баре
    connect(m_muteTimer, &QTimer::timeout, this, &MainWindowCodeEditor::updateMuteTimeDisplayInUserInfo);
    //connect(m_muteTimer, &QTimer::timeout, this, &MainWindowCodeEditor::updateAllUsersMuteTimeDisplay);

    applyCurrentTheme(); // применение темы, по умолчанию темная

    bool ok;
    m_username = QInputDialog::getText(this, tr("Enter Username"), tr("Username:"), QLineEdit::Normal, QDir::home().dirName(), &ok); // окно приложения, заголовок окна (переводимый текст), метка с пояснением для поля ввода, режим обычного текста, начальное значение в поле ввода (имя домашней директории), переменная в которую записывает нажал ли пользователь ОК или нет
    if (!ok || m_username.isEmpty()) { // если пользователь отменил ввод или оставил стркоу пустой, то рандом имя до 999
        m_username = "User" + QString::number(QRandomGenerator::global()->bounded(1000));
    }
    qDebug() << "Set username to" << m_username;
    ui->codeEditor->viewport()->installEventFilter(this); // фильтр чтобы отслеживать изменения размеров viewport

    // подключение сигнала измнения значения вертикального скроллбара
    connect(ui->codeEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindowCodeEditor::onVerticalScrollBarValueChanged);

    // сигнал для "сессий"
    connect(ui->actionCreateSession, &QAction::triggered, this, &MainWindowCodeEditor::onCreateSession);
    connect(ui->actionJoinSession, &QAction::triggered, this, &MainWindowCodeEditor::onJoinSession);
    connect(ui->actionLeaveSession, &QAction::triggered, this, &MainWindowCodeEditor::onLeaveSession);
    connect(ui->actionShowListUsers, &QAction::triggered, this, &MainWindowCodeEditor::onShowUserList);
    ui->actionShowListUsers->setVisible(false);
    ui->actionLeaveSession->setVisible(false);

    // подклчение сигналов от нажатий по пунктам меню к соответствующим функциям
    connect(ui->actionNew_File, &QAction::triggered, this, &MainWindowCodeEditor::onNewFileClicked);
    connect(ui->actionOpen_File, &QAction::triggered, this, &MainWindowCodeEditor::onOpenFileClicked);
    connect(ui->actionOpen_Folder, &QAction::triggered, this, &MainWindowCodeEditor::onOpenFolderClicked);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindowCodeEditor::onSaveFileClicked);
    connect(ui->actionSave_As, &QAction::triggered, this, &MainWindowCodeEditor::onSaveAsFileClicked);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindowCodeEditor::onExitClicked);



    // Инициализация QFileSystemModel (древовидный вид файловой системы слева)
    fileSystemModel = new QFileSystemModel(this); // инициализация модели файловой системы
    fileSystemModel->setRootPath(QDir::homePath()); // задаем корневой путь как домашнюю папку пользователя
    ui->fileSystemTreeView->setModel(fileSystemModel); // привязываем модель к дереву файловой системы
    ui->fileSystemTreeView->setRootIndex(fileSystemModel->index(QDir::homePath())); // устанавливаем корневой индекс (начало отображения) для дерева
    // скрываем лишние столбцы, чтобы отображадась только первая колонка (имена файлов), время, размер и тд скрываются
    ui->fileSystemTreeView->hideColumn(1);
    ui->fileSystemTreeView->hideColumn(2);
    ui->fileSystemTreeView->hideColumn(3);
    // подключаем сигнал двойного клика по элементу дерева к функции, которая будет открывать файл
    connect(ui->fileSystemTreeView, &QTreeView::doubleClicked, this, &MainWindowCodeEditor::onFileSystemTreeViewDoubleClicked);

    m_clientId = QUuid::createUuid().toString();
    qDebug() << "Уникальный идентификатор клиента:" << m_clientId;

    // Сигнал изменения документа клиентом и
    connect(ui->codeEditor->document(), &QTextDocument::contentsChange, this, &MainWindowCodeEditor::onContentsChange);

    connect(ui->codeEditor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindowCodeEditor::onCursorPositionChanged);

    highlighter = new CppHighlighter(ui->codeEditor->document());
}

MainWindowCodeEditor::~MainWindowCodeEditor()
{
    delete ui; // освобождает память, выделенную под интерфейс
    qDeleteAll(remoteCursors); // удаление курсоров всех пользователей
    qDeleteAll(remoteLineHighlights);
    delete socket;
    delete highlighter;
    delete m_themeCheckBox;
    delete m_userListMenu;
    delete m_muteTimer;
    delete m_muteTimeLabel;
}

// пересчет размеров (ширины) всех подсветок строк при измнении размеров окна
bool MainWindowCodeEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->codeEditor->viewport() && event->type() == QEvent::Resize) {
        for (auto it = remoteLineHighlights.begin(); it != remoteLineHighlights.end(); ++it) {
            QString senderId = it.key();
            int position = lastCursorPositions.value(senderId, -1);
            if (position != -1) {
                updateLineHighlight(senderId, position);
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindowCodeEditor::onConnected()
{
    qDebug() << "Клиент успешно подключился к серверу";
    statusBar()->showMessage("Подключено к серверу");

    if (m_sessionId.isEmpty()) {
        qDebug() << "Error: клиент не выбрал или не создал сессию";
        return;
    }

    QJsonObject message;
    if (m_sessionId == "NEW") {
        message["type"] = "create_session";
        message["password"] = m_sessionPassword; // Добавляем пароль
    } else {
        message["type"] = "join_session";
        message["session_id"] = m_sessionId;
        message["password"] = m_sessionPassword; // Добавляем пароль
    }
    message["username"] = m_username;
    message["client_id"] = m_clientId;
    QJsonDocument doc(message);
    socket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void MainWindowCodeEditor::onDisconnected()
{
    statusBar()->showMessage("Отключено от сервера");
    qDebug() << "WebSocket disconnected";

    clearRemoteInfo();
    ui->actionShowListUsers->setVisible(false);
    ui->actionLeaveSession->setVisible(false);
}

void MainWindowCodeEditor::connectToServer()
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState) return; // уже подключены

    if (!socket) {
        // Создаем QWebSocket и подключаем его сигналы
        socket = new QWebSocket();

        // Сигнал "connected" – когда соединение установлено
        connect(socket, &QWebSocket::connected, this, &MainWindowCodeEditor::onConnected);
        connect(socket, &QWebSocket::disconnected, this, &MainWindowCodeEditor::onDisconnected);
        // // Сигнал "errorOccurred" – если возникают ошибки при соединении
        // connect(socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        //     qDebug() << "Ошибка WebSocket:" << socket->errorString();
        //     statusBar()->showMessage("Ошибка: " + socket->errorString());
        // });
        // Сигнал, когда приходит сообщение от сервера
        connect(socket, &QWebSocket::textMessageReceived, this, &MainWindowCodeEditor::onTextMessageReceived);
    }

    socket->open(QUrl("ws://localhost:8080"));
}

void MainWindowCodeEditor::disconnectFromServer()
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        socket->close();
        clearRemoteInfo();
    }
}

bool MainWindowCodeEditor::confirmChangeSession(const QString &message)
{
    QMessageBox msgBoxConfirm(this);
    msgBoxConfirm.setWindowTitle("Confirm");
    msgBoxConfirm.setText(message);
    msgBoxConfirm.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBoxConfirm.setDefaultButton(QMessageBox::No); // кнопки "Нет" по умолчанию
    return msgBoxConfirm.exec() == QMessageBox::Yes;
}

// функция после выхода из сессии
void MainWindowCodeEditor::clearRemoteInfo()
{
    // очистка данных, связанных с сессией
    ui->codeEditor->setPlainText("");
    m_sessionId.clear();
    qDeleteAll(remoteCursors);
    remoteCursors.clear();
    qDeleteAll(remoteLineHighlights);
    remoteLineHighlights.clear();
    remoteUsers.clear();
    statusBar()->clearMessage();
    m_muteTimer->stop();
}

void MainWindowCodeEditor::onCreateSession()
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        if (!confirmChangeSession(tr("Вы уверены, что хотите создать новую сессию? Текущее соединение будет прервано."))) {
            return;
        }
        disconnectFromServer();
    }

    // Запрос пароля для новой сессии
    bool ok;
    QString password = QInputDialog::getText(this, tr("Создание сессии"),
                                             tr("Введите пароль для сессии (мин. 4 символа):"),
                                             QLineEdit::Password, "", &ok);

    if (!ok || password.isEmpty()) {
        return;
    }

    if (password.length() < 4) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Пароль должен содержать минимум 4 символа"));
        return;
    }

    m_sessionPassword = password;
    m_sessionId = "NEW";
    connectToServer();
    ui->actionShowListUsers->setVisible(true);
    ui->actionLeaveSession->setVisible(true);
}
void MainWindowCodeEditor::onJoinSession()
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        if (!confirmChangeSession(tr("Вы уверены, что хотите создать новую сессию? Текущее соединение будет прервано."))) {
            return;
        }
        disconnectFromServer();
    }

    // Запрос ID сессии
    bool ok;
    QString sessionId = QInputDialog::getText(this, tr("Присоединение к сессии"),
                                              tr("Введите ID сессии:"),
                                              QLineEdit::Normal, "", &ok);
    if (!ok || sessionId.isEmpty()) {
        return;
    }

    // Запрос пароля
    QString password = QInputDialog::getText(this, tr("Присоединение к сессии"),
                                             tr("Введите пароль сессии:"),
                                             QLineEdit::Password, "", &ok);
    if (!ok || password.isEmpty()) {
        return;
    }

    m_sessionPassword = password; // Сохраняем пароль
    m_sessionId = sessionId;
    connectToServer();
    ui->actionShowListUsers->setVisible(true);
    ui->actionLeaveSession->setVisible(true);
}

void MainWindowCodeEditor::onLeaveSession()
{
    if (!confirmChangeSession(tr("Вы уверены что хотите покинуть сессию? Подключение будет разорвано, данные пропадут"))) {
        return;
    }

    if (!m_sessionId.isEmpty() && socket->state() == QAbstractSocket::ConnectedState)
    {
        QJsonObject message;
        message["type"] = "leave_session";
        message["client_id"] = m_clientId;
        QJsonDocument doc(message);
        socket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }
    disconnectFromServer();
}

void MainWindowCodeEditor::onOpenFileClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open File"); // открытие диалогового окна для выбора файла
    if (!fileName.isEmpty()) {
        QFile file(fileName); // при непустом файле создается объект для работы с файлом
        if (file.open(QFile::ReadOnly | QFile::Text)) { // открытие файла для чтения в текстовом режиме
            // считывание всего текста из файла и устанавливание в редактор CodeEditor
            QTextStream in(&file);
            ui->codeEditor->setPlainText(in.readAll());
            // закрытие файла и записи пути в переменную
            file.close();
            currentFilePath = fileName;
            QString fileContent = in.readAll();
            // закрытие файла и записи пути в переменную
            file.close();
            currentFilePath = fileName;
            {
                QSignalBlocker blocker(ui->codeEditor->document());
                loadingFile = true;
                ui->codeEditor->setPlainText(fileContent); // установка текста локально
                loadingFile = false;
            }
            highlighter->rehighlight();

            // ОТправка соо на сервер с полным содержимым файла
            QJsonObject fileUpdate;
            fileUpdate["type"] = "file_content_update";
            fileUpdate["text"] = fileContent;
            QJsonDocument doc(fileUpdate);
            QString message = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
            if (socket && socket->state() == QAbstractSocket::ConnectedState)
            {
                socket->sendTextMessage(message);
                qDebug() << "Отправлено сообщение о загрузку файла на сервер";
            }
        } else {
            QMessageBox::critical(this, "Error", "Could not open file");
        }
    }
}

void MainWindowCodeEditor::onSaveFileClicked()
{
    // если путь к файлу пустой, то есть файл ещё не сохранили, то вызывается функция сохранения файла под именем
    if (currentFilePath.isEmpty()) {
        onSaveAsFileClicked();
        return;
    }
    // открытие файла для запаси
    QFile file(currentFilePath);
    if (file.open(QFile::WriteOnly | QFile::Text)) {
        // запись содержимог CodeEditor в файл
        QTextStream out(&file);
        out << ui->codeEditor->toPlainText();
        file.close();
    } else {
        QMessageBox::critical(this, "Error", "Could not save file");
    }
}

void MainWindowCodeEditor::onSaveAsFileClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save As"); // открытие диалогового окна для сохранения файла под другим именем
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QFile::WriteOnly | QFile::Text)) {
            QTextStream out(&file);
            out << ui->codeEditor->toPlainText();
            file.close();
            currentFilePath = fileName;
        }
    } else {
        QMessageBox::critical(this, "Error", "Could not save file");
    }
}

void MainWindowCodeEditor::onExitClicked()
{
    QApplication::quit();
}

void MainWindowCodeEditor::onNewFileClicked()
{
    // очищения поля редактирование и очищение пути к текущему файлу
    ui->codeEditor->clear();
    currentFilePath.clear();
}

void MainWindowCodeEditor::onOpenFolderClicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, "Open Folder"); // диалоговое окно для выбора папки
    if (!folderPath.isEmpty()) {
        QDir dir(folderPath);
        // проверка существует ли папка
        if (dir.exists()) {
            // обновление древовидной модели файловой системы, чтобы ее корневой каталог стал выбранным
            QModelIndex rootIndex = fileSystemModel->setRootPath(folderPath);
            // отображение содержимого выбранной папки
            ui->fileSystemTreeView->setRootIndex(rootIndex);
            // вывод сообщения об открытие папки в строку состояния и в консоль
            statusBar()->showMessage("Opened folder: " + folderPath);
            qDebug() << "Opened folder:" << folderPath;
        } else {
            qDebug() << "Error: выбрана невалидная папка или папка не существует: " << folderPath;
            QMessageBox::critical(this, "Warning", "Выбранная папка несуществует или не является папкой.");
        }
    }
}

void MainWindowCodeEditor::onFileSystemTreeViewDoubleClicked(const QModelIndex &index)
{
    QFileInfo fileInfo = fileSystemModel->fileInfo(index); // опредление, что было выбрано: файл или папка
    if (fileInfo.isFile()) {
        QString filePath = fileInfo.absoluteFilePath(); // если это файл, вы получаем полный путь
        QFile file(filePath);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream in(&file);
            QString fileContent = in.readAll();
            file.close();
            currentFilePath = filePath;
            {
                QSignalBlocker blocker(ui->codeEditor->document());
                loadingFile = true;
                ui->codeEditor->setPlainText(fileContent); // установка текста локально
                loadingFile = false;
            }
            highlighter->rehighlight();

            // ОТправка соо на сервер с полным содержимым файла
            QJsonObject fileUpdate;
            fileUpdate["type"] = "file_content_update";
            fileUpdate["text"] = fileContent;
            QJsonDocument doc(fileUpdate);
            QString message = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
            if (socket && socket->state() == QAbstractSocket::ConnectedState)
            {
                socket->sendTextMessage(message);
                qDebug() << "Отправлено сообщение о загрузку файла на сервер";
            }
            statusBar()->showMessage("Opened file: " + filePath);
        } else {
            QMessageBox::critical(this, "Error", "Could not open file: " + filePath);
        }
    }
}

void MainWindowCodeEditor::onContentsChange(int position, int charsRemoved, int charsAdded) // получает позицию, количество удаленных символов и добавленных символов
{
    if (loadingFile) return;
    if (m_mutedClients.contains(m_clientId) && m_mutedClients.value(m_clientId) != -1) return;
    QJsonObject op; // формирование джсон с информацией
    if (charsAdded > 0)
    {
        op["client_id"] = m_clientId;
        op["type"] = "insert";
        op["position"] = position;
        QString insertedText = ui->codeEditor->toPlainText().mid(position, charsAdded); // извлечение вставленного текста
        op["text"] = insertedText;
    } else if (charsRemoved > 0)
    {
        op["client_id"] = m_clientId;
        op["type"] = "delete";
        op["position"] = position;
        op["count"] = charsRemoved;
    }
    QJsonDocument doc(op);
    QString message = QString::fromUtf8(doc.toJson(QJsonDocument::Compact)); // преобразование джсон в строку и отправка на сервер
    if (socket && socket->state() == QAbstractSocket::ConnectedState)
    {
        socket->sendTextMessage(message);
        qDebug() << "Отправлено сообщение:" << message;
    }
}

void MainWindowCodeEditor::onTextMessageReceived(const QString &message)
{
    qDebug() << "Получено сообщение от сервера:" << message;
    QSignalBlocker blocker(ui->codeEditor->document()); // предотвращение циклического обмена, чтобы примененные изменения снова не отправились на сервер
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject op = doc.object();
    QString opType = op["type"].toString();

    if (opType == "error") {
        QString error = op["message"].toString();
        if (error == "Сессия не найдена") {
            disconnectFromServer();
            QString notFoundSession = op["session_id"].toString();
            statusBar()->showMessage("Сессия с таким идентификатором '" + notFoundSession + "' не найдена");
            QMessageBox::critical(this, "Ошибка", "Сессия не найдена. Проверьте корректность данных");
        }
        else if (error == "Неверный пароль") {
            disconnectFromServer();
            QString sessionId = op.value("session_id").toString();
            statusBar()->showMessage("Неверный пароль для сессии " + sessionId);
            QMessageBox::critical(this, "Ошибка", "Введен неверный пароль для сессии");

            // Предлагаем повторить ввод пароля
            if (!sessionId.isEmpty()) {
                onJoinSession(); // Повторный вызов диалога подключения
            }
        }
        else if (error == "Пароль должен содержать минимум 4 символа") {
            QMessageBox::critical(this, "Ошибка", error);
            onCreateSession(); // Повторный вызов диалога создания сессии
        }
        return;
    } else if (opType == "session_info")
    {
        m_sessionId = op["session_id"].toString();
        statusBar()->showMessage("Session ID: " + m_sessionId);
        qDebug() << "ID session" << m_sessionId;
        m_isAdmin = (op["creator_client_id"].toString() == m_clientId);
        QString fileText = op["text"].toString();
        ui->codeEditor->setPlainText(fileText);
        QJsonObject cursors = op["cursors"].toObject();
        for (const QString& otherClientId : cursors.keys()) {
            QJsonObject cursorInfo = cursors[otherClientId].toObject();
            int position = cursorInfo["position"].toInt();
            QString username = cursorInfo["username"].toString();
            QColor color = QColor(cursorInfo["color"].toString());

            // создаем/обновляем виджеты курсоров и подсветок
            if (!remoteCursors.contains(otherClientId))
            {
                CursorWidget* cursorWidget = new CursorWidget(ui->codeEditor->viewport(), color);
                remoteCursors[otherClientId] = cursorWidget;
                cursorWidget->show();
                cursorWidget->setCustomToolTipStyle(color);
                cursorWidget->setUsername(username);

                LineHighlightWidget* lineHighlight = new LineHighlightWidget(ui->codeEditor->viewport(), color.lighter(150));
                remoteLineHighlights[otherClientId] = lineHighlight;
                lineHighlight->show();
            }
            updateRemoteWidgetGeometry(remoteCursors[otherClientId], position);
            updateLineHighlight(otherClientId, position);

        }
        updateUserListUI();
        highlighter->rehighlight();
        ui->actionShowListUsers->setVisible(true);
        ui->actionLeaveSession->setVisible(true);

    } else if (opType == "user_list_update")
    {
        remoteUsers.clear();
        QJsonArray users = op["users"].toArray();
        for (const QJsonValue& userValue : users) {
            QJsonObject user = userValue.toObject();
            QString clientId = user["client_id"].toString();
            remoteUsers[clientId] = user;

            // Добавляем обработку mute_end_time
            if(user.contains("mute_end_time")) {
                QJsonValue muteEndTimeValue = user["mute_end_time"];
                if(muteEndTimeValue.isNull()){
                    m_muteEndTimes.remove(clientId);
                } else {
                    m_muteEndTimes[clientId] = muteEndTimeValue.toVariant().toLongLong();
                }
            }
        }
        updateUserListUI();

    } else if (opType == "user_disconnected")
    {
        QString disconnectedClientId = op["client_id"].toString();
        QString disconnectedUsername = op["username"].toString();
        qDebug() << "Клиент отключился (уведомление)" << disconnectedUsername << disconnectedClientId;
        statusBar()->showMessage("Клиент отключился (уведомление) " + disconnectedUsername);

        if (remoteCursors.contains(disconnectedClientId)) {
            delete remoteCursors.take(disconnectedClientId);
        }

        if (remoteLineHighlights.contains(disconnectedClientId)) {
            delete remoteLineHighlights.take(disconnectedClientId);
        }

        remoteUsers.remove(disconnectedClientId);
        lastCursorPositions.remove(disconnectedClientId);
        updateUserListUI();

    } else if (opType == "file_content_update")
    {
        QString fileText = op["text"].toString();
        ui->codeEditor->setPlainText(fileText); // замена всего содержимого в редакторе
        qDebug() << "Применено обновление содержимого файла";

    } else if (opType == "chat_message") {
        QString username = op["username"].toString();
        QString clientId = op["client_id"].toString();

        if (op.contains("text_message")) {
            QString chatMessage = op["text_message"].toString();

            if (clientId == m_clientId) {
                chatDisplay->append("<p style='text-align: right;'>" + username + ": " + chatMessage + "</p>");
                //chatDisplay->append("<p style='text-align: left;'></p>");
            } else {
                chatDisplay->append("<p style='text-align: left;'>" + username + ": " + chatMessage + "</p>");
                //chatDisplay->append("<p style='text-align: right;'></p>");
            }
        }
    }else if (opType == "cursor_position_update") {
        QString senderId = op["client_id"].toString();
        if (senderId == m_clientId) return; // игнорирование собственных сообщений

        int position = op["position"].toInt();
        QString username = op["username"].toString();
        QColor color = QColor(op["color"].toString());

        cursorUpdates.append(op);

        if (!remoteCursors.contains(senderId)) // проверка наличия удаленного курсора для данного клиента, если его нет, то он рисуется с нуля
        {
            CursorWidget* cursorWidget = new CursorWidget(ui->codeEditor->viewport(), color); // создается курсор имнено на области отображения текста для правильного позиционирвоания
            remoteCursors[senderId] = cursorWidget;
            cursorWidget->setCustomToolTipStyle(color);
            cursorWidget->show();

            LineHighlightWidget* lineHighlight = new LineHighlightWidget(ui->codeEditor->viewport(), color.lighter(150));
            remoteLineHighlights[senderId] = lineHighlight;
            lineHighlight->show();
        }

        CursorWidget* cursorWidget = remoteCursors[senderId];
        LineHighlightWidget* lineHighlight = remoteLineHighlights[senderId];
        if (cursorWidget && lineHighlight)
        {
            cursorWidget->setUsername(username);
            updateRemoteWidgetGeometry(cursorWidget, position); // обновляем позицию курсора
            updateLineHighlight(senderId, position); // обновляем подсветку строки
        }

    } else if (opType == "mute_notification") {
        int duration = op["duration"].toInt();
        QString messageText;
        if(duration == 0) {
            messageText = tr("Вы бессрочно заглушены и не можете редактировать текст");
        } else {
            messageText = tr("Вы заглушены на %1 секунд. Вы не можете редактировать текст").arg(duration);
        }
        //QMessageBox::warning(this, tr("Заглушены"), messageText);
        QMessageBox *msgBox = new QMessageBox(QMessageBox::Warning, tr("Заглушены"), messageText, QMessageBox::Ok, this);
        msgBox->show();
        updateMutedStatus(); //обновляем мьют, так как это сообщение говорит о том, что пользователя замьютили

    } else if (opType == "muted_status_update") {
        QString mutedClientId = op["client_id"].toString();
        bool mutedClientStatus = op["is_muted"].toBool();
        m_mutedClients[mutedClientId] = mutedClientStatus;

        // Добавляем обработку времени окончания мьюта
        if (op.contains("mute_end_time")) {
            QJsonValue muteEndTimeValue = op["mute_end_time"];
            if (muteEndTimeValue.isNull()) {
                m_muteEndTimes.remove(mutedClientId);
            } else {
                m_muteEndTimes[mutedClientId] = muteEndTimeValue.toVariant().toLongLong();
            }
        }
        // if (mutedClientStatus) {
        //     CursorWidget* cursorWidget = remoteCursors[mutedClientId];
        //     cursorWidget->setVisible(false);
        //     LineHighlightWidget* lineHighlighter = remoteLineHighlights[mutedClientId];
        //     lineHighlighter->setVisible(false);
        // } else {
        //     CursorWidget* cursorWidget = remoteCursors[mutedClientId];
        //     cursorWidget->setVisible(true);
        //     LineHighlightWidget* lineHighlighter = remoteLineHighlights[mutedClientId];
        //     lineHighlighter->setVisible(true);
        // }
        onMutedStatusUpdate(mutedClientId, mutedClientStatus);

    } else if (opType == "admin_changed") {
        QString newAdminId = op["new_admin_id"].toString();
        onAdminChanged(newAdminId);

    } else if (opType == "insert")
    {
        QString senderId = op["client_id"].toString();
        if (m_clientId == senderId) return;

        QString text = op["text"].toString();
        int position = op["position"].toInt();
        QTextCursor cursor(ui->codeEditor->document());
        cursor.setPosition(position);
        cursor.insertText(text);
        qDebug() << "Применена операция вставки";

    } else if (opType == "delete")
    {
        QString senderId = op["client_id"].toString();
        if (m_clientId == senderId) return;

        int count = op["count"].toInt();
        int position = op["position"].toInt();
        QTextCursor cursor(ui->codeEditor->document());
        cursor.setPosition(position);
        cursor.setPosition(position + count, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        qDebug() << "Применена операция удаления";
    }
}

void MainWindowCodeEditor::onCursorPositionChanged()
{
    int cursorPosition = ui->codeEditor->textCursor().position(); // номер символа, где курсор
    QJsonObject cursorUpdate;
    cursorUpdate["type"] = "cursor_position_update";
    cursorUpdate["position"] = cursorPosition;
    cursorUpdate["client_id"] = m_clientId;
    QJsonDocument doc(cursorUpdate); // джсон объект в документ
    QString message = QString::fromUtf8(doc.toJson(QJsonDocument::Compact)); // документ в строку в компактном виде
    if (socket && socket->state() == QAbstractSocket::ConnectedState)
    {
        socket->sendTextMessage(message);
        qDebug() << "Отправлено сообщение о позиции курсора" << message;
    }
}

void MainWindowCodeEditor::onShowUserList()
{
    updateUserListUI(); // обновляем содержмиое меню
    QPoint globalPos = QCursor::pos();
    m_userListMenu->exec(globalPos);
}

void MainWindowCodeEditor::updateUserListUI()
{
    m_userListMenu->clear();

    for (const QString& clientId : remoteUsers.keys()) {
        QJsonObject user = remoteUsers[clientId];
        QString username = user["username"].toString();
        QColor color = QColor(user["color"].toString());
        bool isAdmin = user["is_admin"].toBool();
        bool isMuted = m_mutedClients.contains(clientId) && m_mutedClients.value(clientId) != 0;

        // создаем иконку с цветным кружком
        QPixmap pixmap(13, 13); // размер кружка
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen); // без контура
        painter.drawEllipse(0, 0, 12, 12); // рисуем кружок
        QIcon icon(pixmap);

        // создаем QAction
        QString actionText = username;
        if (isAdmin) {
            actionText += " (Admin)";
        }
        if (isMuted) {
            actionText += tr(" (Muted)");
            // if (m_muteEndTimes.contains(clientId)) {
            //     actionText += " " + formatMuteTime(clientId); // к (Muted) доавбляется форматированный вывод оставшегося времени мьюта
            // }
        }
        QAction *userAction = new QAction(icon, actionText, this); // иконка + текст
        userAction->setData(clientId);

        // создаем меню списка пользователей для каждого пользователя
        QMenu* userContextMenu = new QMenu(this);
        if (m_isAdmin) {
            QAction *muteUnmuteAction = new QAction(isMuted ? tr("Unmute") : tr("Mute"), this);
            QAction *transferAdminAction = new QAction("Transfer Admin Rights", this);
            QAction *userInfoAction = new QAction("Information", this);
            userContextMenu->addAction(muteUnmuteAction);
            userContextMenu->addAction(transferAdminAction);
            userContextMenu->addAction(userInfoAction);

            if (clientId == m_clientId) {
                transferAdminAction->setVisible(false);
                muteUnmuteAction->setVisible(false);
            }

            // подключаем сигналы данных кнопок, используем лямбда-функции для передачи clientId
            connect(muteUnmuteAction, &QAction::triggered, this, [this, clientId]() { onMuteUnmute(clientId); });
            connect(transferAdminAction, &QAction::triggered, this, [this, clientId]() { onTransferAdmin(clientId); });
            connect(userInfoAction, &QAction::triggered, this, [this, clientId]() { showUserInfo(clientId); });
        } else {
            QAction *userInfoAction = new QAction(tr("Information"), this);
            userContextMenu->addAction(userInfoAction);
            connect(userInfoAction, &QAction::triggered, this, [this, clientId]() { showUserInfo(clientId); });
        }
        userAction->setMenu(userContextMenu); // Связываем QAction с QMenu

        m_userListMenu->addAction(userAction);
    }
}

// функция обновления информации об одном конкретном пользователе в списке, вместо полного обновления списка, что снизить нагрузку
void MainWindowCodeEditor::updateUserListUser(const QString& clientId) 
{
    // находим нужный QAction (кнопку в списке меню пользователей), конкретного пользователя
    QAction* userAction = nullptr;
    for (QAction* action : m_userListMenu->actions()) {
        if (action->data().toString() == clientId) {
            userAction - action;
            return;
        }
    }

    if (!userAction) return;

    QJsonObject user = remoteUsers[clientId];
    QString username = user["username"].toString();
    QColor color = user["color"].toString();
    bool isAdmin = user["is_admin"].toBool();
    bool isMuted = m_mutedClients.contains(clientId) && m_mutedClients.value(clientId) != 0;

    // создаем иконку с цветным кружком
    QPixmap pixmap(13, 13); // размер кружка
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen); // без контура
    painter.drawEllipse(0, 0, 12, 12); // рисуем кружок
    QIcon icon(pixmap);

    // создаем QAction
    QString actionText = username;
    if (isAdmin) {
        actionText += " (Admin)";
    }
    if (isMuted) {
        actionText += tr(" (Muted)");
        // if (m_muteEndTimes.contains(clientId)) {
        //     actionText += " " + formatMuteTime(clientId); // к (Muted) доавбляется форматированный вывод оставшегося времени мьюта
        // }
    }
    userAction->setIcon(icon);
    userAction->setText(actionText); // устанавливаем иконку + обновленный текст
}

// обновление позиции и размера курсора (тултипа соответственно)
void MainWindowCodeEditor::updateRemoteWidgetGeometry(QWidget* widget, int position)
{
    if (!widget) return;

    QTextCursor tempCursor(ui->codeEditor->document());
    tempCursor.setPosition(position);
    QRect cursorRect = ui->codeEditor->cursorRect(tempCursor);
    widget->move(cursorRect.topLeft());
    widget->setFixedHeight(cursorRect.height());
}

void MainWindowCodeEditor::updateLineHighlight(const QString& senderId, int position)
{
    if (!remoteLineHighlights.contains(senderId)) return;

    LineHighlightWidget* lineHighlight = remoteLineHighlights[senderId];
    if (!lineHighlight) return;

    QTextCursor tempCursor(ui->codeEditor->document());
    tempCursor.setPosition(position);
    QRect cursorRect = ui->codeEditor->cursorRect(tempCursor);
    lineHighlight->setGeometry(
        0, // х относительно viewport`а
        cursorRect.top(), // y - верхняя граница cursorRect
        ui->codeEditor->viewport()->width(),
        cursorRect.height()
        );
    lineHighlight->setVisible(true);
}

// обновлении позиции подсветки при прокрутке
void MainWindowCodeEditor::onVerticalScrollBarValueChanged(int value)
{
    Q_UNUSED(value);
    // обновляем позицию всех виджетов при прокрутке
    for (auto i = remoteLineHighlights.begin(); i != remoteLineHighlights.end(); ++i)
    {
        QString senderId = "";
        int position = -1; // значение по умолчанию, если позиция курсора для данного клиента не найдена
        for (const auto& cursorUpdate : cursorUpdates)
        {
            if (cursorUpdate["client_id"].toString() == senderId)
            {
                position = cursorUpdate["position"].toInt();
                break;
            }
        }
        if (position != -1)
        {
            updateLineHighlight(senderId, position);
        }
    }
}

void MainWindowCodeEditor::applyCurrentTheme()
{
    if (!m_isDarkTheme) {
        QFile lightFile(":/styles/light.qss");
        if (lightFile.open(QFile::ReadOnly)) {
            QString lightStyle = lightFile.readAll();
            qApp->setStyleSheet(lightStyle);
            chatWidget->setStyleSheet(lightStyle);
            lightFile.close();
            qDebug() << "Light theme applied successfully";
        } else {
            qDebug() << "Failed to open light.qss";
        }
    } else {
        QFile darkFile(":/styles/dark.qss");
        if (darkFile.open(QFile::ReadOnly)) {
            QString darkStyle = darkFile.readAll();
            qApp->setStyleSheet(darkStyle);
            chatWidget->setStyleSheet(darkStyle);
            darkFile.close();
            qDebug() << "Dark theme applied successfully";
        } else {
            qDebug() << "Failed to open dark.qss";
        }
    }
}

void MainWindowCodeEditor::onToolButtonClicked()
{
    m_isDarkTheme = !m_isDarkTheme;
    applyCurrentTheme();
}

void MainWindowCodeEditor::updateMutedStatus()
{
    bool isMuted = m_mutedClients.value(m_clientId);
    if (isMuted) {
        //statusBar()->showMessage("Вы заглушены и не можете писать");
        // отключаем редактирование текста
        qint64 muteEndTime = m_muteEndTimes.value(m_clientId, -1); // Получаем время окончания мьюта. -1 если нет записи
        if (muteEndTime == -1) {
            statusBar()->showMessage(tr("Вы заглушены бессрочно"));
            //QMessageBox::warning(this, "Ошибка", "Вы бессрочно заглушены и не можете редактировать текст");
        }
        else
        {
            // если таймер не запущен, то запускаем
            if (!m_muteTimer->isActive()) {
                m_muteTimer->start(1000);
            }
            updateStatusBarMuteTime(); // вызываем чтобы обновить сразу, а не через секунду
            //qint64 currentTime = QDateTime::currentDateTime().toSecsSinceEpoch(); // Текущее время
            //qint64 timeLeft = muteEndTime - currentTime; // Сколько осталось до конца мьюта в секундах
            //statusBar()->showMessage(tr("Вы заглушены, осталось: %1 сек.").arg(timeLeft));
            //QMessageBox::warning(this, "Ошибка", tr("Вы заглушены и не можете редактировать текст. Осталось: %1 сек.").arg(timeLeft));
        }
        ui->codeEditor->setReadOnly(true);
    } else {
        statusBar()->clearMessage();
        // включаем редактирование текста
        ui->codeEditor->setReadOnly(false);
        m_muteTimer->stop();
    }
}

void MainWindowCodeEditor::onMutedStatusUpdate(const QString &clientId, bool isMuted)
{
    if (isMuted) {
        m_mutedClients[clientId] = 1;
    } else {
        m_mutedClients.remove(clientId);
    }

    if (clientId == m_clientId) {
        updateMutedStatus(); // обновляем статус, когда мьют накладывается на самого пользователя
    }
    updateUserListUI(); // обновляем статус пользователя в списке
}

void MainWindowCodeEditor::updateMuteTimeDisplay(const QString& clientId)
{
    QMessageBox* msgBox = qobject_cast<QMessageBox*>(sender()); //пытаемся получить объект QMessageBox, который отправил сигнал
    if(!msgBox && m_currentMessageBoxClientId.isEmpty()) return; // если не получилось, выходим

    QString currentText;
    if (msgBox) // если был объект, то берем из него текст
    {
        currentText = msgBox->text(); //Если мьюта нет, оставляем, что было, а не затираем пустой строкой
    }

    // если строка равна "", то значит мьюта нет
    QString muteTimeStr = formatMuteTime(clientId);

    int startIndex = currentText.indexOf("<b>Status:</b>");
    if (startIndex != -1) { // если строка найдена, то заменяем
        int endIndex = currentText.indexOf("<br>", startIndex);
        if (endIndex != -1) {
            endIndex = currentText.length(); // если <br> нет, то заменяем до конца строки
        }

        QString statusText;
        if (muteTimeStr.isEmpty()) {
            if (clientId == m_clientId) {
                statusText = tr("Это Вы");
            } else {
                statusText = tr("Не заглушен");
            }
        } else {
            statusText = muteTimeStr; // Осталось: ...
        }

        currentText.replace(startIndex, endIndex - startIndex, "<b>Status:</b> " + statusText);
        if (msgBox) {
            msgBox->setText(currentText); // устанавливаем обновленный текст или оригинальный, если нет мьюта
            qDebug() << currentText;
        }
    }
}

void MainWindowCodeEditor::stopMuteTimer()
{
    m_muteTimer->stop(); // если окно закрылось, то таймер останавливаем
}

// обновление статус бара, вызывается по таймеру
void MainWindowCodeEditor::updateStatusBarMuteTime()
{
    if (m_mutedClients.contains(m_clientId) && m_mutedClients.value(m_clientId) != 0) {
        qint64 muteEndTime = m_muteEndTimes.value(m_clientId);
        if (muteEndTime != -1 && QDateTime::currentDateTime().toSecsSinceEpoch() > muteEndTime) {
            onMutedStatusUpdate(m_clientId, false);
            QMessageBox::information(this, tr("Размьют"), "Вы разблокированы, можете писать в чат");
        }
    }

    if (m_mutedClients.contains(m_clientId) && m_mutedClients.value(m_clientId) != 0) {
        statusBar()->showMessage(formatMuteTime(m_clientId));
    } else {
        statusBar()->clearMessage();
        if (m_muteTimer->isActive()) m_muteTimer->stop();
    }

    // updateUserListUI();
    // обновляем время мьюта для ВСЕХ замьюченыхх пользователей в списке
    for (const QString& clientId : m_muteEndTimes.keys()) {
        if (m_mutedClients.contains(clientId) && m_mutedClients.value(clientId) != 0) { // проверяем что пользователь действительно замьючен
            updateUserListUser(clientId);
        }
    }
}

void MainWindowCodeEditor::onMuteUnmute(const QString targetClientId)
{
    if (targetClientId == m_clientId) return;
    if (m_mutedClients.contains(targetClientId) && m_mutedClients.value(targetClientId) != -1) {
        QJsonObject unmuteMessage;
        unmuteMessage["type"] = "unmute_client";
        unmuteMessage["target_client_id"] = targetClientId;
        unmuteMessage["client_id"] = m_clientId;
        unmuteMessage["session_id"] = m_sessionId;
        QJsonDocument doc(unmuteMessage);
        if (socket && socket->state() == QAbstractSocket::ConnectedState) {
            socket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
            qDebug() << "Отправлен запрос на размут:" << targetClientId;
            onMutedStatusUpdate(targetClientId, false);
        }

    } else {
        bool ok;
        int duration = QInputDialog::getInt(this, tr("Mute User"), tr("Enter mute duration (seconds), 0 - permanently:"), 0, 0, 2147483647, 1, &ok);
        if (ok) {
            QJsonObject muteMessage;
            muteMessage["type"] = "mute_client";
            muteMessage["target_client_id"] = targetClientId;
            muteMessage["duration"] = duration;
            muteMessage["client_id"] = m_clientId;
            muteMessage["session_id"] = m_sessionId;
            QJsonDocument doc(muteMessage);
            if (socket && socket->state() == QAbstractSocket::ConnectedState) {
                socket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
                qDebug() << "Отправлен запрос на мут:" << targetClientId << ", " << duration;
                qint64 endTime = (duration == 0) ? -1 : QDateTime::currentDateTime().toSecsSinceEpoch() + duration;
            m_muteEndTimes[targetClientId] = endTime;
            onMutedStatusUpdate(targetClientId, true); // Обновляем статус сразу
            }
        }
    }
}

// функция для форматирования времени мьюта
QString MainWindowCodeEditor::formatMuteTime(const QString& clientId) 
{
    if (!m_muteEndTimes.contains(clientId)) return ""; // если мьюта нет, то пустую строку возвращаем

    qint64 muteEndTime = m_muteEndTimes.value(clientId);
    if (muteEndTime == -1) return tr("Заглушен бессрочно");

    qint64 currentTime = QDateTime::currentDateTime().toSecsSinceEpoch();
    qint64 timeLeft = muteEndTime - currentTime;

    if (timeLeft <= 0) return tr("");

    int seconds = timeLeft % 60;
    int minutes = (timeLeft / 60) % 60;
    int hours = (timeLeft / 3600) % 24;
    int days = timeLeft / 86400;
    QString timeString;
    if (days > 0) {
        timeString = QString("%1д %2ч %3м %4с").arg(days).arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
    }
    else if (hours > 0) {
        timeString = QString("%1ч %2м %3с").arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
    }
    else
    {
        timeString = QString("%1м %2с").arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
    }

    return tr("Осталось: %1").arg(timeString);
}

void MainWindowCodeEditor::onAdminChanged(const QString &newAdminId)
{
    m_isAdmin = (newAdminId == m_clientId);
    updateUserListUI();
    qDebug() << "Admin status changed. Is admin: " << m_isAdmin;
}

void MainWindowCodeEditor::onTransferAdmin(const QString targetClientId)
{
    if (targetClientId == m_clientId) return;
    QJsonObject transferAdminMessage;
    transferAdminMessage["type"] = "transfer_admin";
    transferAdminMessage["new_admin_id"] = targetClientId;
    transferAdminMessage["client_id"] = m_clientId;
    QJsonDocument doc(transferAdminMessage);
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        socket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }
}

void MainWindowCodeEditor::showUserInfo(const QString targetClientId)
{
    QString username = remoteUsers.value(targetClientId)["username"].toString();
    QString status;
    QString muteTimeInfo;
    QJsonObject user = remoteUsers.value(targetClientId);
    bool isAdmin = false;
    if (!user.isEmpty()) {
        isAdmin = user["is_admin"].toBool();
    }

    bool isMuted = m_mutedClients.contains(targetClientId) && m_mutedClients.value(targetClientId, 0) != 0;
    if (isMuted) {
        if (m_muteEndTimes.contains(targetClientId)) {
            // если клиент замьючен, и есть информация о времени мьюта
            qint64 muteEndTime = m_muteEndTimes.value(targetClientId);
            if (muteEndTime == -1) {
                status = tr("Заглушен бессрочно");
            } else {
                status = formatMuteTime(targetClientId);
                // status = tr("Заглушен");
                // время мьюта будет обновляться динамически
            }
        } else {
            // если мьют есть, но нет времени окончания
            status = tr("Заглушен");
        }
        m_currentMessageBoxClientId = targetClientId; // Сохраняем ID клиента, для которого показываем окно
    } else {
        // если клиент не замьючен
        if (targetClientId == m_clientId) {
            status = tr("Это Вы");
        } else {
            status = tr("Не заглушен");
        }
        m_currentMessageBoxClientId = "";
    }

    QString adminStatus = isAdmin ? "Админ" : "Не админ";

    //QMessageBox::information(this, "User Info", message);
    if (!m_userInfoMessageBox) {
        m_userInfoMessageBox = new QMessageBox(this);

        // создаем таймер для динамического обновления
        QTimer* updateTimer = new QTimer(this);
        connect(updateTimer, &QTimer::timeout, this, &MainWindowCodeEditor::updateMuteTimeDisplayInUserInfo);
        updateTimer->start(1000);

        connect(m_userInfoMessageBox, &QMessageBox::accepted, this, [this, updateTimer]() {
            m_currentUserInfoClientId = ""; // очищаем когда окно закрывается
            m_userInfoMessageBox = nullptr;
            updateTimer->stop();
            updateTimer->deleteLater();
        });
        connect(m_userInfoMessageBox, &QMessageBox::rejected, this, [this, updateTimer]() {
            // для кнопкок "закрыть"
            m_currentUserInfoClientId = "";
            m_userInfoMessageBox = nullptr;
            updateTimer->stop();
            updateTimer->deleteLater();
        });
    }
    m_userInfoMessageBox->setWindowTitle("User Info");
    QString message = QString("<b>Username:</b> %1<br><b>Client ID:</b> %2<br><b>Status:</b> %3<br><b>Admin:</b> %4")
                          .arg(username).arg(targetClientId).arg(status).arg(adminStatus);
    m_userInfoMessageBox->setText(message);
    m_currentUserInfoClientId = targetClientId; // сохраняем айди для обновления
    //QMessageBox *msgBox = new QMessageBox(this);
    //msgBox->setWindowTitle("User Info");
    //msgBox->setText(message);
    //QVBoxLayout *layout = new QVBoxLayout;

    // добавление QLabel'ов в layout
    // QLabel *mainLabel = new QLabel(message);  // текст с основной информацией
    // layout->addWidget(mainLabel);

    // // добавляем QLabel для времени мьюта (m_muteTimeLabel)
    // if (m_muteEndTimes.contains(targetClientId))
    // {
    //     layout->addWidget(m_muteTimeLabel);
    // }
    // msgBox->setLayout(layout);

    /*connect(msgBox, &QMessageBox::close, this, &MainWindowCodeEditor::stopMuteTimer); //останавливаем таймер, когда закрывается*/
    //updateMuteTimeDisplay(targetClientId); // обновляем начальное время мьюта
    updateMuteTimeDisplayInUserInfo();
    m_userInfoMessageBox->show();
}

void MainWindowCodeEditor::updateMuteTimeDisplayInUserInfo()
{
    if (!m_userInfoMessageBox || m_currentUserInfoClientId.isEmpty()) {
        return; // Если окно не открыто или clientId не установлен, выходим
    }

    QString clientId = m_currentUserInfoClientId;
    QString username = remoteUsers.value(clientId)["username"].toString();
    QString status;
    QJsonObject user = remoteUsers.value(clientId);
    bool isAdmin = false;
    if (!user.isEmpty()) {
        isAdmin = user["is_admin"].toBool();
    }
    bool isMuted = m_mutedClients.contains(clientId) && m_mutedClients.value(clientId, 0) != 0;
    if (isMuted) {
        if (m_muteEndTimes.contains(clientId)) {
            // если клиент замьючен, и есть информация о времени мьюта
            qint64 muteEndTime = m_muteEndTimes.value(clientId);
            if (muteEndTime == -1) {
                status = tr("Заглушен бессрочно");
            } else {
                status = formatMuteTime(clientId);
            }
        } else {
            status = tr("Заглушен");
        }
    } else {
        // если клиент не замьючен
        if (clientId == m_clientId) {
            status = tr("Это Вы");
        } else {
            status = tr("Не заглушен");
        }
    }

    QString adminStatus = isAdmin ? "Админ" : "Не админ";
    QString message = QString("<b>Username:</b> %1<br><b>Client ID:</b> %2<br><b>Status:</b> %3<br><b>Admin:</b> %4")
                          .arg(username).arg(clientId).arg(status).arg(adminStatus);

    m_userInfoMessageBox->setText(message); // Обновляем текст в окне
}

// void MainWindowCodeEditor::updateAllUsersMuteTimeDisplay()
// {
//     // если есть открытые окна информации о пользователе
//     if (m_userInfoMessageBox && !m_currentUserInfoClientId.isEmpty()) {
//         updateMuteTimeDisplayInUserInfo();
//     }

//     // обновляем все открытые диалоговые окна с информацией о пользователях
//     for (const QString& clientId : m_muteEndTimes.keys()) {
//         if (m_mutedClients.contains(clientId) && m_mutedClients.value(clientId) != 0) {
//             // получаем все открытые QMessageBox
//             QList<QMessageBox*> messageBoxes = findChildren<QMessageBox*>();
//             for (QMessageBox* msgBox : messageBoxes) {
//                 // обновляем каждое окно
//                 updateMuteTimeDisplay(clientId);
//             }
//         }
//     }
// }

// чатик
void MainWindowCodeEditor::toggleChat() {
    if (chatWidget->isVisible()) {
        chatWidget->hide(); // Скрыть чат
    } else {
        chatWidget->show(); // Показать чат
    }
}

void MainWindowCodeEditor::sendMessage() {
    QString text = chatInput->text().trimmed();
    if (!text.isEmpty()) {
        // Форматируем сообщение
        QString formattedMessage = m_username + ": " + text;

        // Добавляем сообщение в свой чат сразу с выравниванием справа
        chatDisplay->append("<p align='right'>" + formattedMessage + "</p>\n");

        // Подготавливаем JSON для отправки
        QJsonObject chatOp;
        chatOp["type"] = "chat_message";
        chatOp["session_id"] = m_sessionId;
        chatOp["client_id"] = m_clientId;
        chatOp["username"] = m_username;
        chatOp["text_message"] = text;
        QJsonDocument doc(chatOp);
        QString jsonMessage = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

        if (socket && socket->state() == QAbstractSocket::ConnectedState) {
            socket->sendTextMessage(jsonMessage);
            qDebug() << "Отправлено сообщение в чате: " << jsonMessage;
        }
        chatInput->clear();
    }
}
void MainWindowCodeEditor::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        sendMessage();
    }
}
/*void MainWindowCodeEditor::handleIncomingMessage(const QJsonObject &json) {
    QString type = json["type"].toString();

    if (type == "chat_message") {
        // Обработка сообщений чата
        QString username = json["username"].toString();
        QString messageText = json["message"].toString();
        QString formattedMessage = username + ": " + messageText;
        chatDisplay->append(formattedMessage);
    }
}*/

void MainWindowCodeEditor::on_toolButton_clicked()
{
    if (!chatWidget) {
        chatWidget = new QWidget(ui->horizontalWidget_2); // Привязываем к нужному виджету

        if (!ui->horizontalWidget_2->layout()) {
            ui->horizontalWidget_2->setLayout(new QHBoxLayout()); // Если нет лейаута, добавляем
        }

        ui->horizontalWidget_2->layout()->addWidget(chatWidget); // Добавляем чат в нужный слой
    }

    chatWidget->setVisible(!chatWidget->isVisible()); // Переключаем видимость
}


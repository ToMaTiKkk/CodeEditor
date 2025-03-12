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

MainWindowCodeEditor::MainWindowCodeEditor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowCodeEditor)
    , m_isDarkTheme(true)
{
    ui->setupUi(this);
    this->setWindowTitle("CoEdit");
    QFont font("Fira Code", 12);
    QApplication::setFont(font);

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
    QLabel* themeLabel = new QLabel("Тема:", this);
    QHBoxLayout *themeLayout = new QHBoxLayout;
    themeLayout->addWidget(themeLabel);
    themeLayout->addWidget(m_themeCheckBox);
    QWidget *themeWidget = new QWidget(this);
    themeWidget->setLayout(themeLayout);
    ui->menubar->setCornerWidget(themeWidget, Qt::TopRightCorner); // добавляем в правый верхний угол
    applyCurrentTheme();

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
    ui->actionShowListUsers->setEnabled(false);
    ui->actionLeaveSession->setEnabled(false);

    // подклчение сигналов от нажатий по пунктам меню к соответствующим функциям
    connect(ui->actionNew_File, &QAction::triggered, this, &MainWindowCodeEditor::onNewFileClicked);
    connect(ui->actionOpen_File, &QAction::triggered, this, &MainWindowCodeEditor::onOpenFileClicked);
    connect(ui->actionOpen_Folder, &QAction::triggered, this, &MainWindowCodeEditor::onOpenFolderClicked);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindowCodeEditor::onSaveFileClicked);
    connect(ui->actionSave_As, &QAction::triggered, this, &MainWindowCodeEditor::onSaveAsFileClicked);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindowCodeEditor::onExitClicked);

    // установка центрального виджета, чтобы сохранить весь интерфейс
    setCentralWidget(ui->centralwidget);

    // управлние размерами окон для дерева и редактора
    ui->splitter->setStretchFactor(0, 0); // для дерева файлов (левый элемент)
    ui->splitter->setStretchFactor(1, 1); // для окна редактирования (правый элемент)
    // ограничение размера древа файлов
    connect(ui->splitter, &QSplitter::splitterMoved, this, [this](int pos, int index) { // [this] - анонимная лямба-функция, которая может использовать переменные их текущего класса, int pos - позиция сплита от начала, int index - указывает, какой из элементов был перемещен
        int currentTreeViewWidth = ui->fileSystemTreeView->width(); // текущая ширина виджета древа
        int maxAllowedWidth = 350;
        if (currentTreeViewWidth > maxAllowedWidth) {
            QList<int> sizes = ui->splitter->sizes();
            int diff = currentTreeViewWidth - maxAllowedWidth;
            sizes[0] = maxAllowedWidth;
            sizes[1] += diff;
            if (sizes[1] < 0) sizes[1] = 0;
            ui->splitter->setSizes(sizes);
        }
    });

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

    if (m_sessionId.isEmpty())
    {
        qDebug() << "Error: клиент не выбрал или не создал сессию";
        return;
    }
    QJsonObject message;
    if (m_sessionId == "NEW") {
        message["type"] = "create_session";
    } else {
        message["type"] = "join_session";
        message["session_id"] = m_sessionId;
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

    // очистка данных, связанных с сессией
    m_sessionId.clear();
    remoteCursors.clear();
    remoteLineHighlights.clear();
    remoteUsers.clear();
    ui->actionShowListUsers->setEnabled(false);
    ui->actionLeaveSession->setEnabled(false);
    statusBar()->clearMessage();
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
    }
}

void MainWindowCodeEditor::onCreateSession()
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        QMessageBox msgBoxConfirm;
        msgBoxConfirm.setWindowTitle("Confirm");
        msgBoxConfirm.setText("Вы уверены, что хотите создать новую сессию? Текущее соединение будет прервано.");
        QPushButton *yes = msgBoxConfirm.addButton("YES", QMessageBox::AcceptRole);
        QPushButton *no = msgBoxConfirm.addButton("no", QMessageBox::RejectRole);

        if (msgBoxConfirm.clickedButton() == yes) {
            disconnectFromServer();
        } else if (msgBoxConfirm.clickedButton() == no) return;
    }

    m_sessionId = "NEW";
    connectToServer();
    ui->actionShowListUsers->setEnabled(true);
    ui->actionLeaveSession->setEnabled(true);
}

void MainWindowCodeEditor::onJoinSession()
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        QMessageBox msgBoxConfirm;
        msgBoxConfirm.setWindowTitle("Confirm");
        msgBoxConfirm.setText("Вы уверены, что хотите создать новую сессию? Текущее соединение будет прервано.");
        QPushButton *yes = msgBoxConfirm.addButton("YES", QMessageBox::AcceptRole);
        QPushButton *no = msgBoxConfirm.addButton("no", QMessageBox::RejectRole);

        if (msgBoxConfirm.clickedButton() == yes) {
            disconnectFromServer();
        } else if (msgBoxConfirm.clickedButton() == no) return;
    }

    bool ok;
    QString sessionId = QInputDialog::getText(this, tr("Join Session"), tr("Session ID:"), QLineEdit::Normal, "", &ok);
    if (ok && !sessionId.isEmpty()) {
        m_sessionId = sessionId;
        connectToServer(); // подключение с отправкой на сервер соо с join_session
        ui->actionShowListUsers->setEnabled(true);
        ui->actionLeaveSession->setEnabled(true);
    }
}

void MainWindowCodeEditor::onLeaveSession()
{
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
            QMessageBox::warning(this, "Warning", "Выбранная папка несуществует или не является папкой.");
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
    //int position = op["position"].toInt();

    if (opType == "session_info")
    {
        m_sessionId = op["session_id"].toString();
        statusBar()->showMessage("Session ID: " + m_sessionId);
        qDebug() << "ID session" << m_sessionId;
        QString fileText = op["text"].toString();
        ui->codeEditor->setPlainText(fileText);
        QJsonObject cursors = op["cursors"].toObject();
        for (const QString& otherClientId : cursors.keys()) {
            QJsonObject cursorInfo = cursors[otherClientId].toObject();
            int position = cursorInfo["position"].toInt();
            QString username = cursorInfo["username"].toString();
            QColor color = cursorInfo["color"].toString();

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
        highlighter->rehighlight();
        ui->actionShowListUsers->setEnabled(true);
        ui->actionLeaveSession->setEnabled(true);

    } else if (opType == "user_list_update") 
    {
        remoteUsers.clear();
        QJsonArray users = op["users"].toArray();
        for (const QJsonValue& userValue : users) {
            QJsonObject user = userValue.toObject();
            QString clientId = user["client_id"].toString();
            remoteUsers[clientId] = user;
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
        updateUserListUI();

    } else if (opType == "file_content_update")
    {
        QString fileText = op["text"].toString();
        ui->codeEditor->setPlainText(fileText); // замена всего содержимого в редакторе
        qDebug() << "Применено обновление содержимого файла";

    } else if (opType == "cursor_position_update")
    {
        QString senderId = op["client_id"].toString();
        if (senderId == m_clientId) return; // игнорирование собственных сообщений

        int position = op["position"].toInt();
        QString username = op["username"].toString();

        cursorUpdates.append(QJsonDocument::fromJson(message.toUtf8()).object());

        if (!remoteCursors.contains(senderId)) // проверка наличия удаленного курсора для данного клиента, если его нет, то он рисуется с нуля
        {
            // создание виджетов курсора и подсветки строки курсора
            // QStringList colors = QColor::colorNames();
            QStringList colorNames = { "#D81B60", "#8E24AA", "#3949AB", "#00897B", "#F4511E", "#FDD835" };
            QColor cursorColor = QColor(colorNames[remoteCursors.size() % colorNames.size()]); // выбираем цвет на основе количество клиентов, чтобы у каждого был свой цвет
            CursorWidget* cursorWidget = new CursorWidget(ui->codeEditor->viewport(), cursorColor); // создается курсор имнено на области отображения текста для правильного позиционирвоания
            remoteCursors[senderId] = cursorWidget;
            cursorWidget->setCustomToolTipStyle(cursorColor);
            cursorWidget->show();

            LineHighlightWidget* lineHighlight = new LineHighlightWidget(ui->codeEditor->viewport(), cursorColor.lighter(150));
            remoteLineHighlights[senderId] = lineHighlight;
            lineHighlight->show();
        }

        CursorWidget* cursorWidget = remoteCursors[senderId];

        if (cursorWidget)
        {
            cursorWidget->setUsername(username);
            updateRemoteWidgetGeometry(cursorWidget, position); // обновляем позицию курсора
        }
        updateLineHighlight(senderId, position); // обновляем подсветку строки
    
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

}

void MainWindowCodeEditor::updateUserListUI()
{

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
    lineHighlight->setEnabled(true);
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

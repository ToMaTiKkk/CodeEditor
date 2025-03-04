#include "mainwindowcodeeditor.h"
#include "./ui_mainwindowcodeeditor.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtWebSockets/QWebSocket>
#include <QSignalBlocker>
#include <QCoreApplication>
#include <QInputDialog>
#include <QRandomGenerator>

MainWindowCodeEditor::MainWindowCodeEditor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowCodeEditor)
{
    ui->setupUi(this);
    QFile styleFile(":/styles/dark.qss");
    styleFile.open(QFile::ReadOnly);
    QString styleSheet = QLatin1String(styleFile.readAll());
    this->setStyleSheet(styleSheet);
    QFont font("Fira Code", 12);
    QApplication::setFont(font);
    bool ok;
    m_username = QInputDialog::getText(this, tr("Enter Username"), tr("Username:"), QLineEdit::Normal, QDir::home().dirName(), &ok); // окно приложения, заголовок окна (переводимый текст), метка с пояснением для поля ввода, режим обычного текста, начальное значение в поле ввода (имя домашней директории), переменная в которую записывает нажал ли пользователь ОК или нет
    if (!ok || m_username.isEmpty()) { // если пользователь отменил ввод или оставил стркоу пустой, то рандом имя до 999
        m_username = "User" + QString::number(QRandomGenerator::global()->bounded(1000));
    }
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
    // Создаем QWebSocket и подключаем его сигналы
    socket = new QWebSocket();
    // Сигнал "connected" – когда соединение установлено
    connect(socket, &QWebSocket::connected, this, [this]() {
        qDebug() << "Клиент успешно подключился к серверу";
        statusBar()->showMessage("Подключено к серверу");
        QJsonObject usernameMessage;
        usernameMessage["type"] = "set_username";
        usernameMessage["username"] = m_username;
        usernameMessage["Uuid"] = m_clientId;
        QJsonDocument doc(usernameMessage);
        QString message = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->sendTextMessage(message);
            qDebug() << "Send username to server:" << message;
        }
    });
    connect(socket, &QWebSocket::disconnected, this, &MainWindowCodeEditor::onDisconnected);
    // // Сигнал "errorOccurred" – если возникают ошибки при соединении
    // connect(socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
    //     qDebug() << "Ошибка WebSocket:" << socket->errorString();
    //     statusBar()->showMessage("Ошибка: " + socket->errorString());
    // });

    // Сигнал, когда приходит сообщение от сервера
    connect(socket, &QWebSocket::textMessageReceived, this, &MainWindowCodeEditor::onTextMessageReceived);
    socket->open(QUrl("ws://localhost:8080"));
    // Сигнал изменения документа клиентом и
    connect(ui->codeEditor->document(), &QTextDocument::contentsChange, this, &MainWindowCodeEditor::onContentsChange);

    connect(ui->codeEditor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindowCodeEditor::onCursorPositionChanged);
}

MainWindowCodeEditor::~MainWindowCodeEditor()
{
    delete ui; // освобождает память, выделенную под интерфейс
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
            ui->codeEditor->setPlainText(in.readAll());
            file.close();
            currentFilePath = filePath;
            statusBar()->showMessage("Opened file: " + filePath);
        } else {
            QMessageBox::critical(this, "Error", "Could not open file: " + filePath);
        }
    }
}

void MainWindowCodeEditor::onDisconnected()
{
    statusBar()->showMessage("WebSocket disconnected");
    qDebug() << "WebSocket disconnected";
}

void MainWindowCodeEditor::onContentsChange(int position, int charsRemoved, int charsAdded) // получает позицию, количество удаленных символов и добавленных символов
{
    if (loadingFile) return;
    QJsonObject op; // формирование джсон с информацией
    if (charsAdded > 0)
    {
        op["type"] = "insert";
        op["position"] = position;
        QString insertedText = ui->codeEditor->toPlainText().mid(position, charsAdded); // извлечение вставленного текста
        op["text"] = insertedText;
    } else if (charsRemoved > 0)
    {
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
    int position = op["position"].toInt();
    if (opType == "user_disconnected")
    {
        QString disconnectedId = op["Uuid"].toString(); // Uuid == m_clientId
        QString userDisconneted = op["username"].toString();
        if (remoteCursors.contains(disconnectedId))
        {
            CursorWidget* widget = remoteCursors.take(disconnectedId);
            widget->hide();
            widget->deleteLater();
            qDebug() << "Удален курсор для отключенного пользователя:" << userDisconneted;
            statusBar()->showMessage("Пользователь '" + userDisconneted + "' отключился");
        }
    } else if (opType == "file_content_update"){
        QString fileText = op["text"].toString();
        ui->codeEditor->setPlainText(fileText); // замена всего содержимого в редакторе
        qDebug() << "Применено обновление содержимого файла";
    } else if (opType == "cursor_position_update")
    {
        QString senderId = op["client_id"].toString();
        if (senderId == m_clientId) return; // игнорирование собственных сообщений

        int position = op["position"].toInt();
        QString username = op["username"].toString();

        if (!remoteCursors.contains(senderId)) // проверка наличия удаленного курсора для данного клиента, если его нет, то он рисуется с нуля
        {
            // QStringList colors = QColor::colorNames();
            QStringList colorNames = { "#D81B60", "#8E24AA", "#3949AB", "#00897B", "#F4511E", "#FDD835" };
            QColor cursorColor = QColor(colorNames[remoteCursors.size() % colorNames.size()]); // выбираем цвет на основе количество клиентов, чтобы у каждого был свой цвет
            CursorWidget* cursorWidget = new CursorWidget(ui->codeEditor->viewport(), cursorColor); // создается курсор имнено на области отображения текста для правильного позиционирвоания
            remoteCursors[senderId] = cursorWidget;
            cursorWidget->setCustomToolTipStyle(cursorColor);
            cursorWidget->show();
            // LineHighlightWidget* lineHighlight = new LineHighlightWidget(ui->codeEditor->viewport(), cursorColor.lighter(150));
            // remoteLineHighlights[senderId] = lineHighlight;
            // lineHighlight->show();
        }

        CursorWidget* cursorWidget = remoteCursors[senderId];
        LineHighlightWidget* lineHighlight = remoteLineHighlights[senderId];

        if (cursorWidget)
        {
            cursorWidget->setUsername(username);
            QTextCursor textCursor(ui->codeEditor->document());
            textCursor.setPosition(position);
            QRect cursorRect = ui->codeEditor->cursorRect(textCursor); // возвращает прямоугольник с координатами относительно области отображения текста
            //QMargins margins = ui->codeEditor->contentsMargins();
            //cursorRect.translate(margins.left(), margins.top());
            //QPoint widgetPos = ui->codeEditor->viewport()->mapToParent(cursorRect.topLeft());
            //cursorWidget->move(widgetPos);
            cursorWidget->move(cursorRect.topLeft()); // перемещение курсора в начало этого прямоугольника
            cursorWidget->setFixedHeight(cursorRect.height()); // виджет высотой строки
            cursorWidget->setVisible(true);

            // if (lineHighlight)
            // {
            //     lineHighlight->setGeometry(ui->codeEditor->x(), cursorRect.top() + ui->codeEditor, ui->codeEditor->viewport()->rect().width(), cursorRect.height());
            // }
        }
    } else if (opType == "insert")
    {
        QString text = op["text"].toString();
        QTextCursor cursor(ui->codeEditor->document());
        cursor.setPosition(position);
        cursor.insertText(text);
        qDebug() << "Применена операция вставки";
    } else if (opType == "delete")
    {
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

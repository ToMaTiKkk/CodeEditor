#include "mainwindowcodeeditor.h"
#include "./ui_mainwindowcodeeditor.h"
#include "todolistwidget.h"
#include "cursorwidget.h"
#include "linehighlightwidget.h"
#include "sessionparamswindow.h"
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
#include <QTextEdit>
#include <QScrollArea>
#include <QLabel>
#include <QSpacerItem>
#include <QTimer>
#include <QStringList>
#include <QToolTip>

// Константы для чата
const qreal MESSAGE_WIDTH_PERCENT = 75; // Макс. ширина сообщения в % от доступной ширины
const int HORIZONTAL_MARGIN = 10;      // Боковые отступы от краев ScrollArea
const int MESSAGE_SPACING = 5;         // Вертикальный отступ между сообщениями

MainWindowCodeEditor::MainWindowCodeEditor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowCodeEditor)
    , m_isDarkTheme(true)
    , chatWidget(nullptr)
    , chatScrollArea(nullptr)
    , messageListWidget(nullptr)
    , messagesLayout(nullptr)
    , chatInput(nullptr)
    , m_userInfoMessageBox(nullptr)
    , m_muteTimer(new QTimer(this))
    , m_lspManager(nullptr)
    , m_completionWidget(nullptr)
    , m_hoverTimer(nullptr)
    , m_projectRootPath(QDir::homePath()) // корень проект по умолчанию - домашняя папка
    , m_diagnosticTooltip(nullptr)
    , m_isDiagnosticTooltipVisible(false)
{
    ui->setupUi(this);

    setupMainWindow(); // окно и шрифт
    setupCodeEditorArea(); // редактор и нумерация
    setupLsp(); // настраиваем и запускаем LSP
    setupLspCompletionAndHover();
    setupChatWidget(); // чат
    setupUserFeatures(); // меню пользователей, мьют, таймер и тп
    setupMenuBarActions(); // подключение сигналов
    setupFileSystemView(); // дерево файлов
    setupNetwork(); // websocket, client id
    setupThemeAndNick(); // тема и никнейм

    qDebug() << "--- Состояние после ПОЛНОЙ инициализации редактора ---";
    qDebug() << "Splitter widget count:" << ui->splitter->count();
    qDebug() << "Splitter sizes:" << ui->splitter->sizes();
    qDebug() << "--------------------------------------------";
}

void MainWindowCodeEditor::setupMainWindow()
{
    this->setWindowTitle("CodeEdit");
    QFont font("Fira Code", 12);
    QApplication::setFont(font); // шрифт ко всему приложению
}

void MainWindowCodeEditor::setupCodeEditorArea()
{
    // создаем наш собственный виджет окна редактирование QPlainTextEditor
    m_codeEditor = new QPlainTextEdit();
    m_codeEditor->setObjectName("realCodeEditor"); // отладочное имя
    m_codeEditor->setFont(QFont("Fira Code", 12));
    m_codeEditor->setTabStopDistance(25.0);
    m_codeEditor->setFocusPolicy(Qt::StrongFocus); // чтобы мог получать фокус для ввода
    m_codeEditor->setMouseTracking(true);

    lineNumberArea = new LineNumberArea(m_codeEditor); // передаем наш собственный новый редактор, чтобы у класса было понимание, рядом с чем рисовать номера

    // создаем контейнер для редактора и нумерации, объединяем и сливает их в одну единую виджет
    QWidget *codeEditorContainer = new QWidget();
    codeEditorContainer->setObjectName("codeEditorContainer");
    codeEditorContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // виджет может растягиваться
    codeEditorContainer->setMinimumWidth(300);

    QHBoxLayout *layout = new QHBoxLayout(codeEditorContainer); // горизонтальный
    layout->setContentsMargins(0, 0, 0, 0); // отступы внутри контейнера убираем
    layout->setSpacing(0); // убираем зазор между виджетами
    layout->addWidget(lineNumberArea);
    layout->addWidget(m_codeEditor);

    // находим индекс из ui-дизайнера непосредственно codeEditor и заменяем его на созданный контейнер
    int index = ui->splitter->indexOf(ui->codeEditor);
    if (index != -1) { // если нашли
        ui->splitter->replaceWidget(index, codeEditorContainer);
        codeEditorContainer->setVisible(true); // потому что после замены виджет скрывается

        // удаляем прошлый редактор из дизайнера, потмоу что он больше не нужен
        delete ui->codeEditor;
        ui->codeEditor = nullptr; // чтобы случайнно не использовать данный указатель
        qDebug() << "Редактор ui->codeEditor успешно заменен на codeEditorContainer";
    } else {
        ui->splitter->addWidget(codeEditorContainer);
        codeEditorContainer->setVisible(true);
    }

    // подключенеие сигналов к нумерации и окну редактирования
    connect(m_codeEditor->document(), &QTextDocument::blockCountChanged, lineNumberArea, &LineNumberArea::updateLineNumberAreaWidth);
    connect(m_codeEditor, &QPlainTextEdit::updateRequest, lineNumberArea, &LineNumberArea::updateLineNumberArea); // когда пользователь печатает или прокручивает текст, то перерисовывает, только измненную часть нумерации строк, туже самую, что и была
    connect(m_codeEditor->verticalScrollBar(), &QScrollBar::valueChanged, lineNumberArea, QOverload<>::of(&LineNumberArea::update)); // перерисовка полностью, чтобы при скролле  синхронно с текстом сдвигалось

    lineNumberArea->updateLineNumberAreaWidth(); // начальная ширина

    // инициализация подсветки синтаксиса
    highlighter = new CppHighlighter(m_codeEditor->document());
    // фильтр событий для viewport
    m_codeEditor->viewport()->installEventFilter(this);
}

void MainWindowCodeEditor::setupLsp()
{
    // TODO: сделать путь к серверу и язык настраиваемыми
    QString lspExecutable = "clangd";
    QString languageId = "cpp";

    m_lspManager = new LspManager(lspExecutable, this);

    // подключаем сигналы от ЛСП к клиентским слотам
    connect(m_lspManager, &LspManager::serverReady, this, &MainWindowCodeEditor::onLspServerReady);
    connect(m_lspManager, &LspManager::serverStopped, this, &MainWindowCodeEditor::onLspServerStopped);
    connect(m_lspManager, &LspManager::serverError, this, &MainWindowCodeEditor::onLspServerError);
    connect(m_lspManager, &LspManager::diagnosticsReceived, this, &MainWindowCodeEditor::onLspDiagnosticsReceived);
    connect(m_lspManager, &LspManager::completionReceived, this, &MainWindowCodeEditor::onLspCompletionReceived);
    connect(m_lspManager, &LspManager::hoverReceived, this, &MainWindowCodeEditor::onLspHoverReceived);
    connect(m_lspManager, &LspManager::definitionReceived, this, &MainWindowCodeEditor::onLspDefinitionReceived);

    // запускаем сервер для текущего корневого пути проекта
    if (!m_lspManager->startServer(languageId, m_projectRootPath)) {
        qWarning() << "Не удалось запустить LSP сервер для" << m_projectRootPath;
        // TODO: показывать QMessageBox
        statusBar()->showMessage(tr("Не удалось запустить LSP сервер (%1)").arg(lspExecutable), 5000);
    } else {
        statusBar()->showMessage(tr("LSP сервер (%1) запускается").arg(lspExecutable), 3000);
    }
}

void MainWindowCodeEditor::setupLspCompletionAndHover()
{
    m_completionWidget = new CompletionWidget(m_codeEditor, this); // создаем виджет автодополнения
    m_completionWidget->hide();
    connect(m_completionWidget, &CompletionWidget::completionSelected, this, &MainWindowCodeEditor::applyCompletion);

    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true); // срабатывает один раз
    m_hoverTimer->setInterval(700); // задержка в мс перед запросом hover
    connect(m_hoverTimer, &QTimer::timeout, this, &MainWindowCodeEditor::showDiagnoticTooltipOrRequestHover);
}

void MainWindowCodeEditor::setupChatWidget()
{
    chatWidget = new QWidget(ui->horizontalWidget_2);
    chatWidget->setObjectName("chatWidget");

    QVBoxLayout *chatLayout = new QVBoxLayout(chatWidget); // основной layout чата
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(5);

    // Область прокрутки для сообщений
    chatScrollArea = new QScrollArea(chatWidget);
    chatScrollArea->setObjectName("chatScrollArea");
    chatScrollArea->setWidgetResizable(true);
    chatScrollArea->setFrameShape(QFrame::NoFrame);
    // chatScrollArea->setStyleSheet("background-color: transparent;"); // Можно задать прозрачный фон

    // Виджет-контейнер внутри ScrollArea
    messageListWidget = new QWidget();
    messageListWidget->setObjectName("messageListWidget");
    // messageListWidget->setStyleSheet("background-color: transparent;");

    // Вертикальный Layout для сообщений внутри контейнера
    messagesLayout = new QVBoxLayout(messageListWidget);
    messagesLayout->setContentsMargins(HORIZONTAL_MARGIN, 5, HORIZONTAL_MARGIN, 5);
    messagesLayout->setSpacing(MESSAGE_SPACING);
    messagesLayout->addStretch(1); // Заставляет сообщения "расти" вверх

    // Устанавливаем контейнер в ScrollArea
    chatScrollArea->setWidget(messageListWidget);

    chatLayout->addWidget(chatScrollArea, 1); // ScrollArea занимает основное место

    // Layout для ввода сообщения
    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(10);
    inputLayout->setContentsMargins(5, 0, 5, 5);

    chatInput = new QLineEdit(chatWidget);
    chatInput->setObjectName("chatInput");
    chatInput->setPlaceholderText("Введите сообщение...");
    inputLayout->addWidget(chatInput, 1);

    QPushButton *sendButton = new QPushButton(chatWidget);
    sendButton->setObjectName("sendButton");
    QIcon sendIcon(":/styles/send.png");
    sendButton->setIcon(sendIcon);
    sendButton->setIconSize(QSize(24, 24));
    sendButton->setToolTip(tr("Отправить сообщение"));
    sendButton->setCursor(Qt::PointingHandCursor);
    inputLayout->addWidget(sendButton, 0);


    chatLayout->addLayout(inputLayout);
    connect(sendButton, &QPushButton::clicked, this, &MainWindowCodeEditor::sendMessage);
    connect(chatInput, &QLineEdit::returnPressed, this, &MainWindowCodeEditor::sendMessage); // Отправка по Enter

    // Добавляем chatWidget в основной UI
    if (!ui->horizontalWidget_2->layout()) {
        ui->horizontalWidget_2->setLayout(new QHBoxLayout());
        ui->horizontalWidget_2->layout()->setContentsMargins(0,0,0,0);
    }
    ui->horizontalWidget_2->layout()->addWidget(chatWidget);
    chatWidget->hide(); // Скрыть по умолчанию
}

void MainWindowCodeEditor::setupUserFeatures()
{
    // создаем меню для списка пользователей
    m_userListMenu = new QMenu(this);
    ui->actionShowListUsers->setMenu(m_userListMenu);

    //m_muteTimer = new QTimer(this);
    connect(m_muteTimer, &QTimer::timeout, this, &MainWindowCodeEditor::updateStatusBarMuteTime); // вызывает каждую секунду, когда таймер запущен, чтоы обновлять время мьюта в статус-баре
    connect(m_muteTimer, &QTimer::timeout, this, &MainWindowCodeEditor::updateMuteTimeDisplayInUserInfo);
    //connect(m_muteTimer, &QTimer::timeout, this, &MainWindowCodeEditor::updateAllUsersMuteTimeDisplay);
}

void MainWindowCodeEditor::setupMenuBarActions()
{
    // подклчение сигналов от нажатий по пунктам меню к соответствующим функциям
    connect(ui->actionNew_File, &QAction::triggered, this, &MainWindowCodeEditor::onNewFileClicked);
    connect(ui->actionOpen_File, &QAction::triggered, this, &MainWindowCodeEditor::onOpenFileClicked);
    connect(ui->actionOpen_Folder, &QAction::triggered, this, &MainWindowCodeEditor::onOpenFolderClicked);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindowCodeEditor::onSaveFileClicked);
    connect(ui->actionSave_As, &QAction::triggered, this, &MainWindowCodeEditor::onSaveAsFileClicked);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindowCodeEditor::onExitClicked);

    // сигнал для "сессий"
    connect(ui->actionCreateSession, &QAction::triggered, this, &MainWindowCodeEditor::onCreateSession);
    connect(ui->actionJoinSession, &QAction::triggered, this, &MainWindowCodeEditor::onJoinSession);
    connect(ui->actionSaveSession, &QAction::triggered, this, &MainWindowCodeEditor::onSaveSessionClicked);
    connect(ui->actionCopyId, &QAction::triggered, this, &MainWindowCodeEditor::onCopyIdClicked);
    connect(ui->actionLeaveSession, &QAction::triggered, this, &MainWindowCodeEditor::onLeaveSession);
    connect(ui->actionShowListUsers, &QAction::triggered, this, &MainWindowCodeEditor::onShowUserList);
    ui->actionShowListUsers->setVisible(false);
    ui->actionLeaveSession->setVisible(false);
    ui->actionSaveSession->setVisible(false);
    ui->actionCopyId->setVisible(false);

    // connect для Parameters меню
    //connect(ui->actionToDoList, &QAction::triggered, this, &MainWindowCodeEditor::on_actionToDoList_triggered);
    //connect(ui->actionChangeTheme, &QAction::triggered, this, &MainWindowCodeEditor::on_actionChangeTheme_triggered);

    // подключение сигнала измнения значения вертикального скроллбара
    connect(m_codeEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindowCodeEditor::onVerticalScrollBarValueChanged);

    // Сигнал изменения документа клиентом и
    connect(m_codeEditor->document(), &QTextDocument::contentsChange, this, &MainWindowCodeEditor::onContentsChange);
    connect(m_codeEditor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindowCodeEditor::onCursorPositionChanged);
}

void MainWindowCodeEditor::setupFileSystemView()
{
    // Инициализация QFileSystemModel (древовидный вид файловой системы слева)
    fileSystemModel = new QFileSystemModel(this); // инициализация модели файловой системы
    fileSystemModel->setRootPath(QDir::homePath()); // задаем корневой путь как домашнюю папку пользователя
    ui->fileSystemTreeView->setModel(fileSystemModel); // привязываем модель к дереву файловой системы
    ui->fileSystemTreeView->setRootIndex(fileSystemModel->index(QDir::homePath())); // устанавливаем корневой индекс (начало отображения) для дерева

    // скрываем лишние столбцы, чтобы отображадась только первая колонка (имена файлов), время, размер и тд скрываются
    ui->fileSystemTreeView->hideColumn(1);
    ui->fileSystemTreeView->hideColumn(2);
    ui->fileSystemTreeView->hideColumn(3);

    ui->fileSystemTreeView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); //не нужон ваш скролл бар
    ui->fileSystemTreeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // подключаем сигнал двойного клика по элементу дерева к функции, которая будет открывать файл
    connect(ui->fileSystemTreeView, &QTreeView::doubleClicked, this, &MainWindowCodeEditor::onFileSystemTreeViewDoubleClicked);
}

void MainWindowCodeEditor::setupNetwork()
{
    m_clientId = QUuid::createUuid().toString();
    qDebug() << "Уникальный идентификатор клиента:" << m_clientId;
}

void MainWindowCodeEditor::setupThemeAndNick()
{
    applyCurrentTheme(); // применение темы, по умолчанию темная

    m_chatButton = new QPushButton(this);
    m_chatButton->setObjectName("chatButton");

    QIcon chatIcon(":/styles/chat_light.png");
    m_chatButton->setIcon(chatIcon);
    m_chatButton->setIconSize(QSize(24, 24));

    m_chatButton->setToolTip(tr("Открыть/закрыть чат"));
    connect(m_chatButton, &QPushButton::clicked, this, [this](bool checked) {
        Q_UNUSED(checked);
        chatWidget->setVisible(!chatWidget->isVisible());

        if (chatWidget->isVisible()) {
            chatInput->setFocus();
            scrollToBottom();
        }
    });

    QHBoxLayout* chatButtonLayout = new QHBoxLayout;
    chatButtonLayout->addWidget(m_chatButton);
    chatButtonLayout->setContentsMargins(0,0,0,0);

    QWidget* chatWidgetContainer = new QWidget(this);
    chatWidgetContainer->setLayout(chatButtonLayout);

    // Добавляем виджет в правый верхний угол menuBar
    ui->menubar->setCornerWidget(chatWidgetContainer, Qt::TopRightCorner);

    bool ok;
    m_username = QInputDialog::getText(this, tr("Введите никнейм"), tr("Никнейм:"), QLineEdit::Normal, QDir::home().dirName(), &ok); // окно приложения, заголовок окна (переводимый текст), метка с пояснением для поля ввода, режим обычного текста, начальное значение в поле ввода (имя домашней директории), переменная в которую записывает нажал ли пользователь ОК или нет
    if (!ok || m_username.isEmpty()) { // если пользователь отменил ввод или оставил стркоу пустой, то рандом имя до 999
        m_username = "User" + QString::number(QRandomGenerator::global()->bounded(1000));
    }
    qDebug() << "Set username to" << m_username;
}

MainWindowCodeEditor::~MainWindowCodeEditor()
{
    if (m_lspManager) {
        m_lspManager->stopServer();
        m_lspManager = nullptr;
    }

    if (m_trayIcon) {
        m_trayIcon->hide();
        m_trayIcon->deleteLater();
        m_trayIcon = nullptr;
    }

    delete ui; // освобождает память, выделенную под интерфейс
    qDeleteAll(remoteCursors); // удаление курсоров всех пользователей
    qDeleteAll(remoteLineHighlights);
    delete socket;
    delete highlighter;
    delete m_themeCheckBox;
    delete m_userListMenu;
    delete m_muteTimer;
    delete m_muteTimeLabel;
    delete lineNumberArea;
    delete m_codeEditor;
    delete m_completionWidget;
}

void MainWindowCodeEditor::closeEvent(QCloseEvent *event) {
    if (maybeSave()) {
        if (m_lspManager) {
            m_lspManager->stopServer();
        }
        disconnectFromServer();
        event->accept();
    } else {
        event->ignore(); // отмена
    }
}

bool MainWindowCodeEditor::eventFilter(QObject *obj, QEvent *event)
{
    if (m_completionWidget && obj == m_completionWidget) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            qDebug() << "[COMPL_FILTER] KeyPress on CompletionWidget. Key:" << keyEvent->key() << keyEvent->text();
            bool consumedCompletionFilter = true; // по умолчанию считаем что событие обработано и съедено (то есть не передалось никуда)

            switch (keyEvent->key()) {
                case Qt::Key_Up:
                    m_completionWidget->navigateUp();
                    qDebug() << "[COMPL_FILTER] -> Handled Up";
                    break;
                case Qt::Key_Down:
                    m_completionWidget->navigateDown();
                    qDebug() << "[COMPL_FILTER] -> Handled Down";
                    break;
                case Qt::Key_PageUp:
                    m_completionWidget->navigatePageUp();
                    qDebug() << "[COMPL_FILTER] -> Handled PageUp";
                    break;
                case Qt::Key_PageDown:
                    m_completionWidget->navigatePageDown();
                    qDebug() << "[COMPL_FILTER] -> Handled PageDown";
                    break;
                //case Qt::Key_Return:
                //case Qt::Key_Enter:
                case Qt::Key_Tab:
                    m_completionWidget->triggerSelection(); // он скроет виджет и вызовет applyCompletion
                    qDebug() << "[COMPL_FILTER] -> Handled Select (Enter/Tab)";
                    break;
                case Qt::Key_Escape:
                    m_completionWidget->hide();
                    qDebug() << "[COMPL_FILTER] -> Handled Escape";
                    // после скрытия возвращаем фокус редактору
                    QMetaObject::invokeMethod(m_codeEditor, "setFocus", Qt::QueuedConnection);
                    break;
                default:
                    // пересылаем текстовый ввод и другие символы в редактор
                    if (!keyEvent->text().isEmpty() || 
                        keyEvent->key() == Qt::Key_Backspace ||
                        keyEvent->key() == Qt::Key_Delete ||
                        keyEvent->key() == Qt::Key_Space ||
                        (keyEvent->modifiers() != Qt::NoModifier && keyEvent->modifiers() != Qt::ShiftModifier)) // пропускаем модификаторы для шорткатов
                    {
                        qDebug() << "[COMPL_FILTER] -> Default key/text. Forwarding to editor:" << keyEvent->key() << keyEvent->text();
                        // создаем копию события
                        QKeyEvent* forwardEvent = new QKeyEvent(event->type(), keyEvent->key(), keyEvent->modifiers(), keyEvent->text(), keyEvent->isAutoRepeat(), keyEvent->count());
                        // используем асинхронну доставку сообщение редактору
                        QCoreApplication::postEvent(m_codeEditor->viewport(), forwardEvent);
                    }
                    // мы перехватили и переслали (или игнор), поэтому считаем обработанным
                    consumedCompletionFilter = true;
                    break;
            }
            // всегда возвращаем тру, если событие пришло от виджета автодополнения, чтобы не обрабатывалось стандартным образом самим QListWidget 
            return true;
        } else if (event->type() == QEvent::Hide) {
            // перехватываем событие Hide самого виджета
            m_completionWidget->removeEventFilter(this); // удаляем фильтр при скрытии
            if (!m_codeEditor->hasFocus()) {
                QMetaObject::invokeMethod(m_codeEditor, "setFocus", Qt::QueuedConnection);
            }
        }
        return false; // пропускаем другие события такие как мышь и тд
    }

    if (obj == m_codeEditor->viewport()) {
        if (event->type() == QEvent::KeyPress && (!m_completionWidget || !m_completionWidget->isVisible())) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            // используем обработчики для шорткатов, когда автодоп не видно
            handleEditorKeyPressEvent(keyEvent);
            if (keyEvent->isAccepted()) {
                return true;
            }
        } else if (event->type() == QEvent::Resize) {
            for (auto it = remoteLineHighlights.begin(); it != remoteLineHighlights.end(); ++it) {
                QString senderId = it.key();
                int position = lastCursorPositions.value(senderId, -1);
                if (position != -1) {
                    updateLineHighlight(senderId, position);
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            handleEditorMouseMoveEvent(static_cast<QMouseEvent*>(event));
        } else if (event->type() == QEvent::Leave) {
            qDebug() << "[EVENT_FILTER] Mouse left viewport. Hiding tooltip and stopping hover timer.";
            m_hoverTimer->stop();
            if (m_diagnosticTooltip && m_isDiagnosticTooltipVisible) {
                m_diagnosticTooltip->hide();
                m_isDiagnosticTooltipVisible = false;
            }
            QToolTip::hideText();
        }
    }

    // стандартная обработка для всех остальных объектов
    return QMainWindow::eventFilter(obj, event);
}

void MainWindowCodeEditor::handleEditorKeyPressEvent(QKeyEvent *event)
{
    if (!m_codeEditor || !m_codeEditor->viewport()) return;

    // если виджет не виден и событие прошло через дефолт
    if (!event->isAccepted()) { // доп ропеврка, что событие не было съедено
        // если автодополнение неактивно, то проверяем хоткеи
        if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Space) { // Ctrl+пробел
            qDebug() << "  [HANDLER] -> Ctrl+Space detected. Triggering completion.";
            triggerCompletionRequest();
            event->accept(); // съедаем событие, чтобы избежать других действий по Ctrl+пробел
            //return;
        } else if (event->key() == Qt::Key_F12) { // переход к определнию по F12
            // TODO: добавить ctrl+click
            qDebug() << "  [HANDLER] -> F12 detected. Triggering definition.";
            triggerDefinitionRequest();
            event->accept();
            return;
        } else {
            qDebug() << "  [HANDLER] Key not handled by shortcuts.";
        }
    } else {
        qDebug() << "  [HANDLER] Event already accepted before shortcut check.";
   }
}

// обработк движения мыши
void MainWindowCodeEditor::handleEditorMouseMoveEvent(QMouseEvent *event)
{
    if (!m_codeEditor || !m_codeEditor->viewport()) {
        return;
    }

    if ((event->globalPos() - m_lastMousePosForHover).manhattanLength() < 7) {
        return; // дрожание небольшое игнорируем
    }

    // сохраняем позицию мыши (глобальные координаты)
    m_lastMousePosForHover = event->globalPos();
    // запускаем таймер с задержкой, если мышка снова сдвинется, то таймер перезапустится и старый запрос не отправится
    //qDebug() << "[handleEditorMouseMoveEvent] m_hoverTimer are starting...";
    m_hoverTimer->start();

    // скроем существующий кастомный тултип если он видим
    if (m_isDiagnosticTooltipVisible && m_diagnosticTooltip) {
        m_diagnosticTooltip->hide();
        m_isDiagnosticTooltipVisible = false;
    }
    // скроем на всяки и дефолт тултип
    QToolTip::hideText();
}

// !!! слота для обработки сигналов от LspManager !!!
void MainWindowCodeEditor::onLspServerReady()
{
    qInfo() << "LSP сервер готов к работе";
    statusBar()->showMessage(tr("LSP сервер готов"), 3000);
    // если файл уже открыт при старте сервера, то отправляем didOpen
    if (!currentFilePath.isEmpty()) {
        m_currentLspFileUri = getFileUri(currentFilePath);
        m_currentDocumentVersion = 1; // сброс версии
        QString currentText = m_codeEditor->toPlainText();
        if (m_lspManager) {
            m_lspManager->notifyDidOpen(m_currentLspFileUri, currentText, m_currentDocumentVersion);
        }
    }
}

void MainWindowCodeEditor::onLspServerStopped()
{
    qInfo() << "LSP сервер остановлен";
    statusBar()->showMessage(tr("LSP сервер остановлен"), 3000);
    m_diagnostics.clear(); // очищаем старые диагностики
    updateDiagnosticsView(); // убираем подчеркивания
}

void MainWindowCodeEditor::onLspServerError(const QString& message)
{
    qWarning() << "Ошибка LSP сервера:" << message;
    // TODO: выводить окно QMessageBox АСИНХРОННО
    QMessageBox::warning(this, tr("Ошибка LSP"), tr("Произошла ошибка LSP сервера: \n%1").arg(message));
    statusBar()->showMessage(tr("Ошибка LSP сервера!"), 5000);
}

void MainWindowCodeEditor::onLspDiagnosticsReceived(const QString& fileUri, const QList<LspDiagnostic>& diagnostics)
{
    // сохраняем диагностики для данного файла
    //m_diagnostics[fileUri] = diagnostics;

    // если диагностики пришла для ТЕКУЩЕГо открытого файла, то обновляем UI
    if (QUrl(fileUri) == QUrl(m_currentLspFileUri)) {
        qDebug() << "[DIAG] Соответствует URI. Обновляем диагностики и их отображения";
        m_diagnostics[QUrl(m_currentLspFileUri).toString()] = diagnostics;
        updateDiagnosticsView();
    } else {
        qDebug() << "[DIAG] URI НЕ соответствует! Получен:" << QUrl(fileUri).toString() << "Ожидаемый:" << QUrl(m_currentLspFileUri).toString();
        m_diagnostics[QUrl(fileUri).toString()] = diagnostics; // сохраняем на всякий
    }
}

void MainWindowCodeEditor::onLspCompletionReceived(const QList<LspCompletionItem>& items)
{
    if (!m_codeEditor) return;

    if (!m_completionWidget || items.isEmpty()) {
        if (m_completionWidget) {
            m_completionWidget->hide();
        }
        return;
    }

    // вычисление позиции
    QRect currentCursorRect = m_codeEditor->cursorRect(); // получаем актуальную позицию курсора
    // преобразуем координаты в глобальные координаты экрана
    QPoint globalPos = m_codeEditor->viewport()->mapToGlobal(currentCursorRect.bottomLeft());

    // заполняем виджет автодополнения данными
    m_completionWidget->updateItems(items);

    // устанавливаем геометрию (позицию и размер виджета)
    int width = m_completionWidget->sizeHintForColumn(0) + m_codeEditor->verticalScrollBar()->width() + 10; // запа сна скроллбар виджета и отступы
    width = qMax(300, width); // минимальная ширина
    int height = m_completionWidget->sizeHintForRow(0) * qMin(10, m_completionWidget->count()) + 5; // высота примерно 10 элементов + рамка
    height = qMin(300, height); // максимальная высота, что не перекрывать полэкрана
    m_completionWidget->setGeometry(globalPos.x(), globalPos.y(), width, height);

    m_completionWidget->installEventFilter(this); // все события сначала будут проверять MainWIndowCOdeEditor, а потом уже нужные будут отправляться в виджет
    m_completionWidget->show();
    m_completionWidget->raise(); //поверх других виджетов
    //m_completionWidget->setFocus(); // передаем фокус при навигаации клавиатурой
    //qDebug() << "[onLspCompletionReceived] Completion shown. Filter installed.";
}

void MainWindowCodeEditor::onLspHoverReceived(const LspHoverInfo& hoverInfo)
{
    if (hoverInfo.contents.isEmpty()) {
        // TODO: если пришла пустая инфа, можно скрыть тултип
        // QToolTip::hidetext(); // ненадежно, не всегда работает
        return;
    }

    // преобразуем глобальную позициб мышки в локальную для viewport
    QPoint viewportPos = m_codeEditor->viewport()->mapFromGlobal(m_lastMousePosForHover);
    QRect cursorRect = m_codeEditor->cursorRect(); // берем высоту из курсора
    QRect tipRect(viewportPos.x(), viewportPos.y(), 1, cursorRect.height()); // прямоугольник для позиционирования тултипа
    QToolTip::showText(m_lastMousePosForHover, hoverInfo.contents.toHtmlEscaped().replace("\n", "<br>"), m_codeEditor->viewport());
}

void MainWindowCodeEditor::onLspDefinitionReceived(const QList<LspDefinitionLocation>& locations)
{
    if (locations.isEmpty()) {
        statusBar()->showMessage(tr("Определение не найдено"), 3000);
        return;
    }

    // INFO: пока что переходим к первому найденному определению
    const LspDefinitionLocation& loc = locations.first();
    QString targerFilePath = getLocalPath(loc.fileUri); // конвектируем в локальный путь
    qInfo() << "Переход к определению:" << targerFilePath << "строка" << loc.line << "символ" << loc.character;

    // проверяем является ли файл с определением текушим открытым файлом
    if (targerFilePath == currentFilePath) {
        if (m_lspManager && m_codeEditor && m_codeEditor->document()) {
            int targetPos = m_lspManager->lspPosToEditorPos(m_codeEditor->document(), loc.line, loc.character);
            if (targetPos != -1) {
                QTextCursor cursor = m_codeEditor->textCursor();
                cursor.setPosition(targetPos);
                m_codeEditor->setTextCursor(cursor);
                m_codeEditor->ensureCursorVisible(); // прокуртить к курсору
                m_codeEditor->setFocus();
            } else {
                qWarning() << "Не удалось конвертировать позицию LSP в позицию редактора для определения";
            }
        }
    } else {
        // открываем другой файл
        qInfo() << "Нужно открыть файл:" << targerFilePath;
        // TODO: реализовать открытие файла и перемещени курсора после открытия
        statusBar()->showMessage(tr("Определение в другом файле (открытие пока не реализовано): %1").arg(targerFilePath), 5000);
    }
}

void MainWindowCodeEditor::applyCompletion(const QString& textToInsert)
{
    if (!m_codeEditor) return;

    QTextCursor cursor = m_codeEditor->textCursor();
    // TODO: более умная вставка, чтобы может указывался диапозон который нужно заменить и какой текст вставить
    // просто вставляем как есть
    cursor.insertText(textToInsert);
    m_codeEditor->setFocus(); // возвращаем фокус редактору
    qDebug() << "[applyCompletion] Completion applied, focus set to m_codeEditor";
}

void MainWindowCodeEditor::showDiagnoticTooltipOrRequestHover()
{
    // проверяем, существует ли редактор и находится ли мышь над его viewport
    if (!m_codeEditor || !m_codeEditor->viewport() || !m_codeEditor->viewport()->rect().contains(m_codeEditor->viewport()->mapFromGlobal(m_lastMousePosForHover))) {
        // мышь ушла из окна видимости во время таймера или редактора нет
        if (m_diagnosticTooltip && m_isDiagnosticTooltipVisible) {
            m_diagnosticTooltip->hide();
            m_isDiagnosticTooltipVisible = false;
        }
        QToolTip::hideText();
        return;
    }

    QPoint viewportPos = m_codeEditor->viewport()->mapFromGlobal(m_lastMousePosForHover);
    QTextCursor cursor = m_codeEditor->cursorForPosition(viewportPos);
    int currentPos = cursor.position();

    QString diagnosticMessage;
    QList<QTextEdit::ExtraSelection> currentSelections = m_codeEditor->extraSelections(); // они были установлены в updateDiagnosticsView

    // ищем первую диагностику под курсором
    for (const auto& selection : currentSelections) {
        // проверяем если ли у формата кастомное пользовательское свойство
        QVariant messageData = selection.format.property(QTextFormat::UserProperty + 1);
        if (messageData.isValid() && !messageData.toString().isEmpty()) {
            if (currentPos >= selection.cursor.selectionStart() && currentPos < selection.cursor.selectionEnd()) {
                diagnosticMessage = messageData.toString();
                break; // выходим, потому что показываем только первый найденный
            }
        }
    }

    // если тултип диагностики не показали, то обычный можно будет запросить
    if (!diagnosticMessage.isEmpty()) {
        qDebug() << "[HoverTimer] Diagnostic found:" << diagnosticMessage << ". Showing custom tooltip.";
        // создаем тултип если его не было ешё
        if (!m_diagnosticTooltip) {
            m_diagnosticTooltip = new DiagnosticTooltip(m_codeEditor->viewport());

            // убедимся что создался
            if (!m_diagnosticTooltip) {
                qCritical() << "[HoverTimer] Failed to create DiagnosticTooltip!";
                return;
            }
        }

        m_diagnosticTooltip->setText(diagnosticMessage);
        //qDebug() << "[HoverTimer] Tooltip text set. Calculated size:" << m_diagnosticTooltip->sizeHint() << m_diagnosticTooltip->size(); // Посмотрим размер
        // позиционируем рядом с мышкой
        QPoint tooltipPos = m_lastMousePosForHover + QPoint(10, 15); // чуть ниже и правее
        // TODO: добавить чтобы за экране не уходил
        //qDebug() << "[HoverTimer] Moving tooltip to globalPos:" << tooltipPos;
        m_diagnosticTooltip->move(tooltipPos);
        //qDebug() << "[HoverTimer] Calling show(). Current visible state:" << m_diagnosticTooltip->isVisible() << "isDiagnosticTooltipVisible flag:" << m_isDiagnosticTooltipVisible;
        m_diagnosticTooltip->show();
        m_isDiagnosticTooltipVisible = true;
        //qDebug() << "[HoverTimer] After show(). Current visible state:" << m_diagnosticTooltip->isVisible() << "isDiagnosticTooltipVisible flag:" << m_isDiagnosticTooltipVisible;

        QToolTip::hideText();
    } else {
        qDebug() << "[HoverTimer] No diagnostic found. Hiding custom tooltip and potentially requesting LSP hover.";
        if (m_diagnosticTooltip && m_isDiagnosticTooltipVisible) {
            m_diagnosticTooltip->hide();
            m_isDiagnosticTooltipVisible = false;
            //qDebug() << "[HoverTimer] After hide(). Visible state:" << m_diagnosticTooltip->isVisible() << "isDiagnosticTooltipVisible flag:" << m_isDiagnosticTooltipVisible;
        }
        QToolTip::hideText();

        // ЛОГИКА ОБЫЧНОГО ЗАПРОСА LSP HOVER
        // проверяем существует ли редактор и находится ли мышь над его viewport
        if (!m_lspManager || !m_lspManager->isReady() || m_currentLspFileUri.isEmpty() || !m_codeEditor->document()) {
            return;
        }

        qDebug() << "[HoverTimer] Requesting LSP hover.";
        // заапрашиваем всплывашку для позиции, сохраненный в m_lasrMousePosForhover
        //int editorPos = m_codeEditor->cursorForPosition(viewportPos).position();
        QPoint lspPos = m_lspManager->editorPosToLspPos(m_codeEditor->document(), currentPos);

        if (lspPos.x() != -1) { // проверка корректности позиции
            m_lspManager->requestHover(m_currentLspFileUri, lspPos.x(), lspPos.y());
        }
    }
}

// функция для вызова запроса автодополнения
void MainWindowCodeEditor::triggerCompletionRequest()
{
    if (!m_lspManager || !m_lspManager->isReady() || m_currentLspFileUri.isEmpty() || !m_codeEditor->document()) {
        if (m_completionWidget) {
            m_completionWidget->hide();
        }
        return;
    }
    QTextCursor cursor = m_codeEditor->textCursor();
    int editorPos = cursor.position();
    QPoint lspPos = m_lspManager->editorPosToLspPos(m_codeEditor->document(), editorPos);

    if (lspPos.x() != -1) {
        // отправляем запрос TriggerKind = 1 (вызвано вручную)
        m_lspManager->requestCompletion(m_currentLspFileUri, lspPos.x(), lspPos.y(), 1);
    } else {
        if (m_completionWidget) {
            m_completionWidget->hide();
        }
    }
}

// функция для вызова запроса определения
void MainWindowCodeEditor::triggerDefinitionRequest()
{
    if (!m_lspManager || !m_lspManager->isReady() || m_currentLspFileUri.isEmpty() || !m_codeEditor->document()) {
        return;
    }

    QTextCursor cursor = m_codeEditor->textCursor();
    int editorPos = cursor.position();
    QPoint lspPos = m_lspManager->editorPosToLspPos(m_codeEditor->document(), editorPos);

    if (lspPos.x() != -1) {
        m_lspManager->requestDefinition(m_currentLspFileUri, lspPos.x(), lspPos.y());
    }
}

// обновляет подчеркивания ошибок в редакторе
void MainWindowCodeEditor::updateDiagnosticsView()
{
    if (!m_codeEditor || !m_lspManager) {
        qDebug() << "[UPDATE_DIAG_VIEW] Aborted: editor or lspManager is null.";
        return;
    }

    // Получаем актуальный URI для логов
    QString currentUriStr = QUrl(m_currentLspFileUri).toString();
    qDebug() << "[UPDATE_DIAG_VIEW] Called for URI:" << currentUriStr;

    QList<QTextEdit::ExtraSelection> extraSelections; // список подчеркивания
    // берем диагностики для ТЕКУЩЕГо файла из хранилища
    const QList<LspDiagnostic>& currentFileDiagnostics = m_diagnostics.value(QUrl(m_currentLspFileUri).toString());

    //qDebug() << "[UPDATE_DIAG_VIEW] Diagnostics count found in map:" << currentFileDiagnostics.count();

    for (const LspDiagnostic& diag : currentFileDiagnostics) {
        qDebug() << "[UPDATE_DIAG_VIEW] Processing diag: msg='" << diag.message
                 << "', line:" << diag.startLine << ", char:" << diag.startChar
                 << "-> line:" << diag.endLine << ", char:" << diag.endChar
                 << ", severity:" << diag.severity;

        QTextEdit::ExtraSelection selection;

        // устанавливаем цвет и стиль подчеркивания
        QColor color;
        QTextCharFormat::UnderlineStyle style = QTextCharFormat::WaveUnderline; // оставим волнистую или выберем другую
        QColor backgroundColor = Qt::transparent; // по умолчанию прозрачный
        if (diag.severity == 1){
            color = QColor(255, 80, 80); // Error
            backgroundColor = QColor(255, 0, 0, 20); // красный с низкой прозрачностью
        } else if (diag.severity == 2) {
            color = QColor(255, 190, 0); // Warning (оранж)
            backgroundColor = QColor(255, 190, 0, 20);
        } else if (diag.severity == 3) {
            color = QColor(100, 150, 255); // Info
            style = QTextCharFormat::DotLine;
        } else {
            color = Qt::gray;
            style = QTextCharFormat::DotLine;
        }

        selection.format.setBackground(backgroundColor);
        selection.format.setUnderlineColor(color);
        selection.format.setUnderlineStyle(style);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true); // Попробуем на всякий случай

        // КОНВЕРТАЦИЯ устанавливаем курсор на диапозон диагностики
        int startPos = m_lspManager->lspPosToEditorPos(m_codeEditor->document(), diag.startLine, diag.startChar);
        int endPos = m_lspManager->lspPosToEditorPos(m_codeEditor->document(), diag.endLine, diag.endChar);

        // проверяем корректность позиций
        if (startPos != -1 && endPos != -1 && startPos <= endPos) {
            QTextCursor cursor(m_codeEditor->document());
            cursor.setPosition(startPos);
            if (startPos == endPos && endPos < m_codeEditor->document()->characterCount() -1) {
                qDebug() << "[UPDATE_DIAG_VIEW]   Zero-length range, extending endPos by 1 (tentative).";
                endPos++; // если сервер дает что начало равно концу для одного символа
            }

            // выделяем текст от начала до конца
            cursor.setPosition(endPos, QTextCursor::KeepAnchor);
            if (cursor.isNull()) {
                qWarning() << "[UPDATE_DIAG_VIEW]   Failed to create valid cursor for positions:" << startPos << "->" << endPos;
            } else {
                selection.cursor = cursor;
                // сообщение для тултипа
                QString tooltipMessage = QString("[%1] %2").arg(diag.severity).arg(diag.message);
                // сохраняем как пользовательское свойство с уникальным айди
                selection.format.setProperty(QTextFormat::UserProperty + 1, tooltipMessage);
                qDebug() << "[UPDATE_DIAG_VIEW]   Added ExtraSelection with range:" << selection.cursor.selectionStart() << "->" << selection.cursor.selectionEnd() << "and stored message:" << tooltipMessage;;
                extraSelections.append(selection);
            }
        } else {
            qWarning() << "Не удалось отобразить диагностику: неверный диапозон" << diag.startLine << diag.startChar << "-" << diag.endLine << diag.endChar;
        }
    }

    qDebug() << "[UPDATE_DIAG_VIEW] Setting" << extraSelections.count() << "extra selections to the editor.";
    // применяем созданный список подчеркиваний к редактору
    m_codeEditor->setExtraSelections(extraSelections);
}

QString MainWindowCodeEditor::getFileUri(const QString& localPath) const
{
    return QUrl::fromLocalFile(localPath).toString();
}

QString MainWindowCodeEditor::getLocalPath(const QString& fileUri) const
{
    return QUrl(fileUri).toLocalFile();
}

// !!! слоты для обработки сигналов сессий !!!
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
        if (!pendingSessionSave.isEmpty()) {
            QJsonDocument saveDoc = QJsonDocument::fromJson(pendingSessionSave);
            socket->sendTextMessage(QString::fromUtf8(saveDoc.toJson(QJsonDocument::Compact)));
            pendingSessionSave.clear();
        }
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
    ui->actionSaveSession->setVisible(false);
    ui->actionCopyId->setVisible(false);
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
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
        connect(socket, &QWebSocket::textMessageReceived, this, &MainWindowCodeEditor::onTextMessageReceived);
    }

    socket->open(QUrl("ws://YOUR_WEBSOCKET_HOST:YOUR_WEBSOCKET_PORT"));
}

void MainWindowCodeEditor::disconnectFromServer()
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        socket->close();
        clearRemoteInfo();
    }
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
}

bool MainWindowCodeEditor::confirmChangeSession(const QString &message)
{
    QMessageBox msgBoxConfirm(this);
    msgBoxConfirm.setWindowTitle("Подтверждение");
    msgBoxConfirm.setText(message);
    msgBoxConfirm.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBoxConfirm.setDefaultButton(QMessageBox::No); // кнопки "Нет" по умолчанию
    return msgBoxConfirm.exec() == QMessageBox::Yes;
}

// функция после выхода из сессии
void MainWindowCodeEditor::clearRemoteInfo()
{
    // очистка данных, связанных с сессией
    m_codeEditor->setPlainText("");
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
    if (!maybeSave()) {
        return; // нажали отмену
    }

    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        if (!confirmChangeSession(tr("Вы уверены, что хотите создать новую сессию? Текущее соединение будет прервано."))) {
            return;
        }
        disconnectFromServer();
    }
    SessionParamsWindow paramsWindow(this);
    if (paramsWindow.exec() != QDialog::Accepted) {
        return;
    }
    QString password = paramsWindow.getPassword();
    int saveDays = paramsWindow.getSaveDays();
    if (password.length() < 4) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Пароль должен содержать минимум 4 символа"));
        return;
    }

    m_sessionPassword = password;
    m_sessionId = "NEW";
     m_pendingSaveDays = saveDays;
    connectToServer();
}

void MainWindowCodeEditor::onJoinSession()
{
    if (!maybeSave()) {
        return; // нажали отмену
    }

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

void MainWindowCodeEditor::onSaveSessionClicked() {
    if (!m_isAdmin) {
        QMessageBox::warning(this, "Ошибка", "Только администратор может сохранить сессию");
        return;
    }

    bool ok;
    int days = QInputDialog::getInt(this, tr("Сохранение сессии"),
                                    tr("На сколько дней сохранить сессию?"),
                                    1, 1, 7, 1, &ok);
    if (!ok) return;

    QJsonObject saveSessionMessage;
    saveSessionMessage["type"] = "save_session";
    saveSessionMessage["client_id"] = m_clientId;
    saveSessionMessage["session_id"] = m_sessionId;
    saveSessionMessage["days"] = days;
    QJsonDocument doc(saveSessionMessage);

    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        socket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }
}

void MainWindowCodeEditor::onOpenFileClicked()
{
    if (!maybeSave()) {
        return; // нажали отмену
    }

    QString fileName = QFileDialog::getOpenFileName(this, "Открытить файл"); // открытие диалогового окна для выбора файла
    if (!fileName.isEmpty()) {
        QFile file(fileName); // при непустом файле создается объект для работы с файлом
        QString fileContent;
        if (file.open(QFile::ReadOnly | QFile::Text)) { // открытие файла для чтения в текстовом режиме
            // LSP закрываем предыдущий файл
            if (m_lspManager && m_lspManager->isReady() && !m_currentLspFileUri.isEmpty()) {
                m_lspManager->notifyDidClose(m_currentLspFileUri);
            }
            m_diagnostics.clear();
            updateDiagnosticsView();

            // считывание всего текста из файла и устанавливание в редактор CodeEditor
            QTextStream in(&file);
            fileContent = in.readAll();
            file.close();
            currentFilePath = fileName; // сохраняем локальный путь
            {
                QSignalBlocker blocker(m_codeEditor->document());
                loadingFile = true;
                if (m_mutedClients.contains(m_clientId)) return; // если замьючен, то локально текст не обновится
                m_codeEditor->setPlainText(fileContent); // установка текста локально
                lineNumberArea->updateLineNumberAreaWidth(); // пересчитать ширину
                lineNumberArea->update(); // принудительно перерисовать область номеров
                loadingFile = false;
            }
            highlighter->rehighlight();

            // LSP открываем новый файл
            m_currentLspFileUri = getFileUri(currentFilePath);
            m_currentDocumentVersion = 1;
            if (m_lspManager && m_lspManager->isReady() && !m_currentLspFileUri.isEmpty()) {
                qDebug() << ">>> Вызов notifyDidOpen для:" << m_currentLspFileUri;
                m_lspManager->notifyDidOpen(m_currentLspFileUri, fileContent, m_currentDocumentVersion);
            }

            // ОТправка соо на сервер с полным содержимым файла
            QJsonObject fileUpdate;
            fileUpdate["type"] = "file_content_update";
            fileUpdate["text"] = fileContent;
            fileUpdate["client_id"] = m_clientId;
            fileUpdate["username"] = m_username;
            QJsonDocument doc(fileUpdate);
            QString message = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
            if (socket && socket->state() == QAbstractSocket::ConnectedState)
            {
                socket->sendTextMessage(message);
                qDebug() << "Отправлено сообщение о загрузку файла на сервер";
            }
        } else {
            QMessageBox::critical(this, "ОШИБКА", "Невозможно открыть файл");
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
        QString currentText = m_codeEditor->toPlainText();
        out << currentText;
        file.close();
        m_codeEditor->document()->setModified(false);
        statusBar()->showMessage(tr("Файл сохранен: %1").arg(currentFilePath), 2000);
    } else {
        QMessageBox::critical(this, "ОШИБКА", "Невозможно сохранить файл");
    }
}

void MainWindowCodeEditor::onSaveAsFileClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Сохранить как"); // открытие диалогового окна для сохранения файла под другим именем
    if (!fileName.isEmpty()) {
        // LSP закрываем предыдущий файл
        if (m_lspManager && m_lspManager->isReady() && !m_currentLspFileUri.isEmpty()) {
            m_lspManager->notifyDidClose(m_currentLspFileUri);
        }

        QFile file(fileName);
        if (file.open(QFile::WriteOnly | QFile::Text)) {
            QTextStream out(&file);
            QString currentText = m_codeEditor->toPlainText();
            out << currentText;
            file.close();
            bool wasNewFile = currentFilePath.isEmpty(); // запоминаем был ли это новый файл
            currentFilePath = fileName;
            m_codeEditor->document()->setModified(false);

            // LSP отправляем didOpen, если это было первое сохранени
            QString newUri = getFileUri(currentFilePath);
            if (m_lspManager && m_lspManager->isReady() && wasNewFile) {
                m_currentLspFileUri = newUri;
                m_currentDocumentVersion = 1;
                m_lspManager->notifyDidOpen(m_currentLspFileUri, currentText, m_currentDocumentVersion);
            } else if (m_lspManager && m_lspManager->isReady() && newUri != m_currentLspFileUri) {
                // если файл был пересохранен под другим именем, закрываем старый URI и открываем новый
                if (m_lspManager && m_lspManager->isReady() && !m_currentLspFileUri.isEmpty()) {
                    m_lspManager->notifyDidClose(m_currentLspFileUri);
                }
                m_currentLspFileUri = newUri;
                m_currentDocumentVersion = 1;
                m_lspManager->notifyDidOpen(m_currentLspFileUri, currentText, m_currentDocumentVersion);
            }
            statusBar()->showMessage(tr("Файл сохранен: %1").arg(currentFilePath), 2000);
        } else {
            QMessageBox::critical(this, "ОШИБКА", "Невозможно сохранить файл");
        }
    }
}

bool MainWindowCodeEditor::maybeSave()
{
    if (!m_codeEditor->document()->isModified())
        return true;
    QMessageBox::StandardButton ret;
    ret = QMessageBox::warning(this, tr("Предупреждение"),
                               tr("Документ был изменен.\n"
                                  "Хотите сохранить изменения?"),
                               QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (ret == QMessageBox::Save) {
        // если есть имя файла, сохраняем, иначе сохраняем как
        if (!currentFilePath.isEmpty()) {
            onSaveFileClicked();
            return !m_codeEditor->document()->isModified(); // true, если сохранение сбросила флаг модификации
        } else {
            onSaveAsFileClicked();
            // проверяем сохранился ли файл
            return !currentFilePath.isEmpty() && !m_codeEditor->document()->isModified();
        }
    } else if (ret == QMessageBox::Cancel) {
        return false; // Отмена закрытия
    }

    return true;
}

void MainWindowCodeEditor::onExitClicked()
{
    if (!maybeSave()) {
        return; // нажали отмену
    }
    QApplication::quit();
}

void MainWindowCodeEditor::onNewFileClicked()
{
    if (!maybeSave()) {
        return; // нажали отмену
    }
    if (m_mutedClients.contains(m_clientId)) return; // если замьючен, то локально текст не обновится

    // LSP закрываем предыдущий файл
    if (m_lspManager && m_lspManager->isReady() && !m_currentLspFileUri.isEmpty()) {
        m_lspManager->notifyDidClose(m_currentLspFileUri);
    }
    m_diagnostics.clear();
    updateDiagnosticsView();

    // очищения поля редактирование и очищение пути к текущему файлу
    m_codeEditor->clear();
    currentFilePath.clear();
    m_currentLspFileUri.clear();
    m_currentDocumentVersion = 0;
    m_codeEditor->document()->setModified(false);

    // TODO: реализовать генерацию временного URI, чтобы для нового и несохраненного файла иметь Lsp

    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject fileUpdate;
        fileUpdate["type"] = "file_content_update";
        fileUpdate["text"] = "";
        fileUpdate["client_id"] = m_clientId;
        fileUpdate["username"] = m_username;
        QJsonDocument doc(fileUpdate);
        QString message = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        socket->sendTextMessage(message);
        qDebug() << "Отправлено сообщении о создании нового файла (очистке)";
    }
    statusBar()->showMessage(tr("Новый файл создан"), 2000);
}

void MainWindowCodeEditor::onCopyIdClicked()
{
    if (m_sessionId.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Сессия не активна или ID отсутствует.");
        return;
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(m_sessionId);

    // Можно показать уведомление, что ID скопирован
    statusBar()->showMessage("ID сессии скопирован в буфер обмена", 3000); // 3 секунды
}

void MainWindowCodeEditor::onOpenFolderClicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, "Открыть папку"); // диалоговое окно для выбора папки
    if (!folderPath.isEmpty()) {
        QDir dir(folderPath);
        // проверка существует ли папка
        if (dir.exists()) {
            // обновление древовидной модели файловой системы, чтобы ее корневой каталог стал выбранным
            QModelIndex rootIndex = fileSystemModel->setRootPath(folderPath);
            // отображение содержимого выбранной папки
            ui->fileSystemTreeView->setRootIndex(rootIndex);

            // перезапуск LSP для новой папки
            m_projectRootPath = folderPath; // обновили корень проекта
            m_diagnostics.clear();
            updateDiagnosticsView();
            currentFilePath.clear(); // считаем что файл не открыт
            m_currentDocumentVersion = 0;
            m_codeEditor->clear();
            m_codeEditor->document()->setModified(false);

            if (m_lspManager) {
                m_lspManager->stopServer(); // останавливаем старый сервер
                // запускаем новый
                if (!m_lspManager->startServer("cpp", m_projectRootPath)) {
                    qWarning() << "Не удалось перезапустить LSP сервер для" << m_projectRootPath;
                    statusBar()->showMessage(tr("Не удалось перезапустить LSP сервер"), 5000);
                } else {
                    statusBar()->showMessage(tr("LSP сервер перезапускается для папки %1...").arg(folderPath), 3000);
                }
            }

            // вывод сообщения об открытие папки в строку состояния и в консоль
            statusBar()->showMessage("Открыта папка: " + folderPath);
            qDebug() << "Открыта папка:" << folderPath;
        } else {
            qDebug() << "Error: выбрана невалидная папка или папка не существует: " << folderPath;
            QMessageBox::warning(this, "Предупреждение", "Выбранная папка несуществует или не является папкой.");
        }
    }
}

void MainWindowCodeEditor::onFileSystemTreeViewDoubleClicked(const QModelIndex &index)
{
    QFileInfo fileInfo = fileSystemModel->fileInfo(index); // опредление, что было выбрано: файл или папка
    if (fileInfo.isFile()) {
        if (!maybeSave()) {
            return; // нажали отмену
        }
        QString filePath = fileInfo.absoluteFilePath(); // если это файл, вы получаем полный путь
        QFile file(filePath);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            // LSP закрываем предыдущий файл
            if (m_lspManager && m_lspManager->isReady() && !m_currentLspFileUri.isEmpty()) {
                m_lspManager->notifyDidClose(m_currentLspFileUri);
            }
            m_diagnostics.clear();
            updateDiagnosticsView();

            QTextStream in(&file);
            QString fileContent = in.readAll();
            file.close();
            currentFilePath = filePath;
            {
                QSignalBlocker blocker(m_codeEditor->document());
                loadingFile = true;
                if (m_mutedClients.contains(m_clientId)) return; // если замьючен, то локально текст не обновится
                m_codeEditor->setPlainText(fileContent); // установка текста локально
                lineNumberArea->updateLineNumberAreaWidth(); // пересчитать ширину
                lineNumberArea->update(); // принудительно перерисовать область номеров
                loadingFile = false;
            }
            highlighter->rehighlight();

            // LSP открываем новый файл
            m_currentLspFileUri = getFileUri(currentFilePath);
            m_currentDocumentVersion = 1;
            if (m_lspManager && m_lspManager->isReady() && !m_currentLspFileUri.isEmpty()) {
                qDebug() << ">>> Вызов notifyDidOpen для:" << m_currentLspFileUri;
                m_lspManager->notifyDidOpen(m_currentLspFileUri, fileContent, m_currentDocumentVersion);
            }

            // ОТправка соо на сервер с полным содержимым файла
            QJsonObject fileUpdate;
            fileUpdate["type"] = "file_content_update";
            fileUpdate["text"] = fileContent;
            fileUpdate["client_id"] = m_clientId;
            fileUpdate["username"] = m_username;
            QJsonDocument doc(fileUpdate);
            QString message = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
            if (socket && socket->state() == QAbstractSocket::ConnectedState)
            {
                socket->sendTextMessage(message);
                qDebug() << "Отправлено сообщение о загрузку файла на сервер";
            }
            statusBar()->showMessage("Открыт файл: " + filePath);
        } else {
            QMessageBox::critical(this, "ОШИБКА", "Невозможно открыть файл: " + filePath);
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
        QString insertedText = m_codeEditor->toPlainText().mid(position, charsAdded); // извлечение вставленного текста
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

    if (m_lspManager && m_lspManager->isReady() && !m_currentLspFileUri.isEmpty()) {
        m_currentDocumentVersion++;
        QString currentText = m_codeEditor->toPlainText();
        m_lspManager->notifyDidChange(m_currentLspFileUri, currentText, m_currentDocumentVersion);

        // авто-запрос автодополнения после точки или ->
        QTextCursor cursor = m_codeEditor->textCursor();
        if (cursor.position() > 0 && charsAdded > 0) {
            QString currentText = m_codeEditor->toPlainText();
            QChar lastChar = currentText.at(cursor.position() - 1);
            bool shouldTrigger = false;

            // тригеры от сервера
            // TODO: получать конкретный список тригеров при инициализации!!! пока что хардим
            const QString triggerChars = ".:>";
            if (triggerChars.contains(lastChar)) {
                if (lastChar == QLatin1Char(':')) {
                    if (cursor.position() > 1 && currentText.at(cursor.position() - 2) == QLatin1Char(':')) {
                        shouldTrigger = true;
                    }
                } else if (lastChar == QLatin1Char('>')) {
                    if (cursor.position() > 1 && currentText.at(cursor.position() - 2) == QLatin1Char('>')) {
                        shouldTrigger = true;
                    }
                } else if (lastChar == QLatin1Char('.')) {
                    shouldTrigger = true;
                }
            }

            // тригер при начале или продолжении ввода идентификатора (буквы, цифры, _)
            if (lastChar.isLetterOrNumber() || lastChar == QLatin1Char('_')) {
                // проверяем символ перед последним введем для контекста
                if (cursor.position() == 1) { // если это самый первый символ в документе
                    shouldTrigger = true;
                } else {
                    QChar charBeforeLast = currentText.at(cursor.position() - 2);
                    // тригерим и проверяем, а вдруг начали новое слово или предыдущий символ тоже буква или цифра или _, то есть продолжаем слово (для фильтрации)
                    if (!charBeforeLast.isLetterOrNumber() && charBeforeLast != QLatin1Char('_')) {
                        // начали новое слова после пробела, скобки, оператора и тп
                        shouldTrigger = true;
                    } else {
                        // TODO: фильтровать список на клиенте, дабы сервер не нагружать каждый раз
                        if (m_completionWidget && m_completionWidget->isVisible()) {
                            shouldTrigger = true; // перезапросим для обновление списка
                        }
                    }
                }
            }

            if (shouldTrigger) {
                // задержка чтобы не спамить сервер быстрым набором
                QTimer::singleShot(300, this, &MainWindowCodeEditor::triggerCompletionRequest);
                // singleShot чтобы не блокать ввод!
            } else if (m_completionWidget && m_completionWidget->isVisible()) {
                // если ввели символ который не должен тригерить запро, пока что просто скроем (условно после пробела, точки с запятой)
                if (!lastChar.isLetterOrNumber() && lastChar != QLatin1Char('_') && !triggerChars.contains(lastChar)) {
                    m_completionWidget->hide();
                } else {
                // TODO: ФИЛЬТРАЦИЮ КЛИЕНТСКУЮ сделать уже существуещего списка
                }
            }
        } else if (charsRemoved > 0 && m_completionWidget && m_completionWidget->isVisible()) {
            // пользователь удалил символ
            // TODO: обновляем и перефилтровываем список
            m_completionWidget->hide();
        }
    }
    m_codeEditor->document()->setModified(true);
}

void MainWindowCodeEditor::onTextMessageReceived(const QString &message)
{
    qDebug() << "Получено сообщение от сервера:" << message;
    //QSignalBlocker blocker(m_codeEditor->document()); // предотвращение циклического обмена, чтобы примененные изменения снова не отправились на сервер
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
        m_codeEditor->setPlainText(fileText);
        QJsonObject cursors = op["cursors"].toObject();
        for (const QString& otherClientId : cursors.keys()) {
            QJsonObject cursorInfo = cursors[otherClientId].toObject();
            int position = cursorInfo["position"].toInt();
            QString username = cursorInfo["username"].toString();
            QColor color = QColor(cursorInfo["color"].toString());

            // создаем/обновляем виджеты курсоров и подсветок
            if (!remoteCursors.contains(otherClientId))
            {
                CursorWidget* cursorWidget = new CursorWidget(m_codeEditor->viewport(), color);
                remoteCursors[otherClientId] = cursorWidget;
                cursorWidget->show();
                cursorWidget->setCustomToolTipStyle(color);
                cursorWidget->setUsername(username);

                LineHighlightWidget* lineHighlight = new LineHighlightWidget(m_codeEditor->viewport(), color.lighter(150));
                remoteLineHighlights[otherClientId] = lineHighlight;
                lineHighlight->show();
            }
            updateRemoteWidgetGeometry(remoteCursors[otherClientId], position);
            updateLineHighlight(otherClientId, position);

        }
        updateUserListUI();
        highlighter->rehighlight();

        ui->actionSaveSession->setVisible(m_isAdmin);
        ui->actionCopyId->setVisible(true);
        ui->actionShowListUsers->setVisible(true);
        ui->actionLeaveSession->setVisible(true);
        if (m_pendingSaveDays > 0) {
            QJsonObject saveSessionMessage;
            saveSessionMessage["type"] = "save_session";
            saveSessionMessage["client_id"] = m_clientId;
            saveSessionMessage["session_id"] = m_sessionId; // Используем ПОЛУЧЕННЫЙ ID
            saveSessionMessage["days"] = m_pendingSaveDays;
            QJsonDocument saveDoc(saveSessionMessage);

            if (socket && socket->state() == QAbstractSocket::ConnectedState) {
                socket->sendTextMessage(QString::fromUtf8(saveDoc.toJson(QJsonDocument::Compact)));
                qDebug() << "Отправлен запрос на сохранение сессии" << m_sessionId << "на" << m_pendingSaveDays << "дней (после создания)";
            } else {
                qDebug() << "Ошибка: Не удалось отправить запрос на сохранение сессии после создания (сокет не подключен?)";
            }
            m_pendingSaveDays = 0; // сброс флага
        }

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
        QSignalBlocker blocker(m_codeEditor->document());
        QString fileText = op["text"].toString();
        m_codeEditor->setPlainText(fileText); // замена всего содержимого в редакторе
        qDebug() << "Применено обновление содержимого файла";

    } else if (opType == "chat_message") {
        QString username = op["username"].toString();
        QString chatMessage = op["text_message"].toString();
        addChatMessageWidget(username, chatMessage, QTime::currentTime(), false); // false - не свое сообщение

        if (!chatWidget->isVisible() || this->isMinimized()) {

            if (!m_trayIcon) { //m_trayIcon == nullptr) {
                m_trayIcon = new QSystemTrayIcon(this);
                connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, [this]() { // раскрытие чата при клике на уведомление
                    if (this->isMinimized()) {
                        this->showNormal();
                        this->activateWindow();
                    }
                    if (!chatWidget->isVisible()) {
                        chatWidget->setVisible(true);
                        chatInput->setFocus();
                        scrollToBottom();
                    }
                });
            }
            m_trayIcon->setIcon(QIcon(":/styles/chat_light.png"));
            m_trayIcon->show();
            QString title = "У вас новое сообщение от " + username; // Заголовок уведомления
            QString message = chatMessage;
            m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
        }
    } else if (opType == "cursor_position_update") {
        QString senderId = op["client_id"].toString();
        if (senderId == m_clientId) return; // игнорирование собственных сообщений

        int position = op["position"].toInt();
        QString username = op["username"].toString();
        QColor color = QColor(op["color"].toString());

        cursorUpdates.append(op);

        if (!remoteCursors.contains(senderId)) // проверка наличия удаленного курсора для данного клиента, если его нет, то он рисуется с нуля
        {
            CursorWidget* cursorWidget = new CursorWidget(m_codeEditor->viewport(), color); // создается курсор имнено на области отображения текста для правильного позиционирвоания
            remoteCursors[senderId] = cursorWidget;
            cursorWidget->setCustomToolTipStyle(color);
            cursorWidget->show();

            LineHighlightWidget* lineHighlight = new LineHighlightWidget(m_codeEditor->viewport(), color.lighter(150));
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
                if (m_clientId == mutedClientId) {
                    QMessageBox::information(this, tr("Размьют"), tr("Вы разблокированы, можете редактировать текст!"));
                }
            } else {
                m_muteEndTimes[mutedClientId] = muteEndTimeValue.toVariant().toLongLong();
            }
        }
        onMutedStatusUpdate(mutedClientId, mutedClientStatus);

    } else if (opType == "admin_changed") {
        QString newAdminId = op["new_admin_id"].toString();
        onAdminChanged(newAdminId);

    } else if (opType == "insert")
    {
        QSignalBlocker blocker(m_codeEditor->document());
        QString senderId = op["client_id"].toString();
        if (m_clientId == senderId) return;

        QString text = op["text"].toString();
        int position = op["position"].toInt();
        QTextCursor cursor(m_codeEditor->document());
        cursor.setPosition(position);
        cursor.insertText(text);
        qDebug() << "Применена операция вставки";

    } else if (opType == "delete")
    {
        QSignalBlocker blocker(m_codeEditor->document());
        QString senderId = op["client_id"].toString();
        if (m_clientId == senderId) return;

        int count = op["count"].toInt();
        int position = op["position"].toInt();
        QTextCursor cursor(m_codeEditor->document());
        cursor.setPosition(position);
        cursor.setPosition(position + count, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        qDebug() << "Применена операция удаления";
    } else if (opType == "session_saved") {
        int days = op["days"].toInt();
        QMessageBox::information(this, "Успех",
                                 QString("Сессия сохранена на %1 дней").arg(days));
    }
}


void MainWindowCodeEditor::onCursorPositionChanged()
{
    int cursorPosition = m_codeEditor->textCursor().position(); // номер символа, где курсор
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
            actionText += " (Админ)";
        }
        if (isMuted) {
            actionText += tr(" (Мьют)");
        }
        QAction *userAction = new QAction(icon, actionText, this); // иконка + текст
        userAction->setData(clientId);

        // создаем меню списка пользователей для каждого пользователя
        QMenu* userContextMenu = new QMenu(this);
        if (m_isAdmin) {
            QAction *muteUnmuteAction = new QAction(isMuted ? tr("Размьют") : tr("Мьют"), this);
            QAction *transferAdminAction = new QAction("Передать права админа", this);
            QAction *userInfoAction = new QAction("Информация", this);
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
            QAction *userInfoAction = new QAction(tr("Информация"), this);
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
            userAction = action;
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
        actionText += " (Админ)";
    }
    if (isMuted) {
        actionText += tr(" (Мьют)");
    }
    userAction->setIcon(icon);
    userAction->setText(actionText); // устанавливаем иконку + обновленный текст
}

// обновление позиции и размера курсора (тултипа соответственно)
void MainWindowCodeEditor::updateRemoteWidgetGeometry(QWidget* widget, int position)
{
    if (!widget) return;

    QTextCursor tempCursor(m_codeEditor->document());
    tempCursor.setPosition(position);
    QRect cursorRect = m_codeEditor->cursorRect(tempCursor);
    widget->move(cursorRect.topLeft());
    widget->setFixedHeight(cursorRect.height());
}

void MainWindowCodeEditor::updateLineHighlight(const QString& senderId, int position)
{
    if (!remoteLineHighlights.contains(senderId)) return;

    LineHighlightWidget* lineHighlight = remoteLineHighlights[senderId];
    if (!lineHighlight) return;

    QTextCursor tempCursor(m_codeEditor->document());
    tempCursor.setPosition(position);
    QRect cursorRect = m_codeEditor->cursorRect(tempCursor);
    lineHighlight->setGeometry(
        0, // х относительно viewport`а
        cursorRect.top(), // y - верхняя граница cursorRect
        /*m_codeEditor->viewport()->width(),*/
        10000,
        cursorRect.height()
        );
    // устанавливаем видимость в зависимости от статуса мьюта
    bool isMuted = m_mutedClients.contains(senderId) && m_mutedClients.value(senderId, 0) != 0;
    lineHighlight->setVisible(!isMuted);
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
        // отключаем редактирование текста
        qint64 muteEndTime = m_muteEndTimes.value(m_clientId, -1); // Получаем время окончания мьюта. -1 если нет записи
        if (muteEndTime == -1) {
            statusBar()->showMessage(tr("Вы заглушены бессрочно"));
        }
        else
        {
            // если таймер не запущен, то запускаем
            if (!m_muteTimer->isActive()) {
                m_muteTimer->start(1000);
            }
            updateStatusBarMuteTime(); // вызываем чтобы обновить сразу, а не через секунду
        }
        m_codeEditor->setReadOnly(true);
    } else {
        statusBar()->clearMessage();
        // включаем редактирование текста
        m_codeEditor->setReadOnly(false);
        m_muteTimer->stop();
    }
}

void MainWindowCodeEditor::onMutedStatusUpdate(const QString &clientId, bool isMuted)
{
    //bool wasMuted = m_mutedClients.contains(clientId) && m_mutedClients.value(clientId, 0) != 0;
    if (isMuted) {
        m_mutedClients[clientId] = 1;
    } else {
        m_mutedClients.remove(clientId);
        m_muteEndTimes.remove(clientId);
    }

    if (clientId != m_clientId) {
        LineHighlightWidget* lineHighlight = remoteLineHighlights.value(clientId, nullptr);
        if (lineHighlight) {
            lineHighlight->setVisible(!isMuted); // Показываем, если НЕ замьючен
        }
    }
    if (clientId == m_clientId) {
        updateMutedStatus(); // обновляем статус, когда мьют накладывается на самого пользователя
        // if (wasMuted != isMuted) {
        //     // QMessageBox *msgBox = new QMessageBox(this);
        //     // msgBox->information(this, tr("Размьют"), "Вы разблокированы, можете редактировать текст!");
        //     // msgBox->show();
        //     QMessageBox::information(this, tr("Размьют"), tr("Вы разблокированы, можете редактировать текст!"));
        // }
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

    int startIndex = currentText.indexOf("<b>Статус:</b>");
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

        currentText.replace(startIndex, endIndex - startIndex, "<b>Статусs:</b> " + statusText);
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
            QMessageBox::information(this, tr("Размьют"), "Вы разблокированы, можете редактировать текст");
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
        int duration = QInputDialog::getInt(this, tr("Мьюь пользователя"), tr("Выберите длительность мьюта (секунды), 0 - бессрочный:"), 0, 0, 2147483647, 1, &ok);
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
    ui->actionSaveSession->setVisible(m_isAdmin); // обновляем видимость кнопки
    updateUserListUI(); // добавляет или убирает приписку с админом в списоке пользователей
    qDebug() << "Admin status changed. Is admin: " << m_isAdmin;

    if (m_isAdmin) {
        statusBar()->showMessage(tr("Вы теперь администратор сессии"), 3000);
    } else {
        QString newAdminUsername = tr("Другой пользователь");
        if (remoteUsers.contains(newAdminId)) {
            newAdminUsername = remoteUsers[newAdminId].value("username").toString();
        }
        statusBar()->showMessage(tr("Администратором сессии стал %1").arg(newAdminUsername), 3000);
    }
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
        status = tr("Заглушен бессрочно");
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
    m_userInfoMessageBox->setWindowTitle("Инфо о пользователе");
    QString message = QString("<b>Никнейм:</b> %1<br><b>Client ID:</b> %2<br><b>Статус:</b> %3<br><b>Админ:</b> %4")
                          .arg(username).arg(targetClientId).arg(status).arg(adminStatus);
    m_userInfoMessageBox->setText(message);
    m_currentUserInfoClientId = targetClientId; // сохраняем айди для обновления
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
            status = formatMuteTime(clientId);
        } else {
            status = tr("Заглушен бессрочно");
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
    QString message = QString("<b>Никнейм:</b> %1<br><b>Client ID:</b> %2<br><b>Статус:</b> %3<br><b>Админ:</b> %4")
                          .arg(username).arg(clientId).arg(status).arg(adminStatus);

    m_userInfoMessageBox->setText(message); // Обновляем текст в окне
}

void MainWindowCodeEditor::sendMessage() {
    QString text = chatInput->text().trimmed();
    if (!text.isEmpty()) {
        // Используем "You" для отображения своих сообщений
        addChatMessageWidget("Вы", text, QTime::currentTime(), true); // true - свое сообщение

        QJsonObject chatOp;
        chatOp["type"] = "chat_message";
        chatOp["session_id"] = m_sessionId;
        chatOp["client_id"] = m_clientId;
        chatOp["username"] = m_username; // Отправляем реальное имя пользователя
        chatOp["text_message"] = text;
        QJsonDocument doc(chatOp);
        QString jsonMessage = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

        if (socket && socket->state() == QAbstractSocket::ConnectedState) {
            socket->sendTextMessage(jsonMessage);
            qDebug() << "Отправлено сообщение в чате: " << jsonMessage;
        } else {
            qDebug() << "Ошибка: сокет не подключен, не могу отправить сообщение.";
            // Можно добавить системное сообщение об ошибке в чат
            addChatMessageWidget("System", "Error: Not connected to server.", QTime::currentTime(), false);
        }
        chatInput->clear(); // Очищаем поле ввода
    }
}

// Слот для прокрутки (вызывается через QTimer::singleShot)
void MainWindowCodeEditor::scrollToBottom() {
    if (chatScrollArea) {
        // Задержка может быть нужна, чтобы layout успел обновиться
        QTimer::singleShot(50, [this]() { // Небольшая задержка 50 мс
            if (chatScrollArea) { // Проверка на случай, если виджет удален за время задержки
                chatScrollArea->verticalScrollBar()->setValue(chatScrollArea->verticalScrollBar()->maximum());
            }
        });
    }
}

// для добавления виджета сообщения
void MainWindowCodeEditor::addChatMessageWidget(const QString &username, const QString &text, const QTime &time, bool isOwnMessage)
{
    if (!messagesLayout || !chatScrollArea) return;

    auto formatMessageText = [](const QString& text, int maxWidth) -> QString {
        QFontMetrics fm(QApplication::font());
        QString result;
        QString currentLine;

        for (const QChar& c : text) {
            QString testLine = currentLine + c;
            if (fm.horizontalAdvance(testLine) <= maxWidth) {
                currentLine = testLine;
            } else {
                if (!currentLine.isEmpty()) {
                    if (!currentLine.endsWith(' ')) {
                        currentLine += '-';
                    }
                    result += currentLine + '\n';
                }
                currentLine = c;
            }
        }
        result += currentLine;
        return result.replace('\n', "<br>").replace(" ", "&nbsp;");
    };

    QLabel *messageLabel = new QLabel();
    messageLabel->setProperty("messageType", isOwnMessage ? "own" : "other");
    messageLabel->setObjectName("chatMessageLabel");

    messageLabel->setWordWrap(true);
    messageLabel->setTextFormat(Qt::RichText);
    messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    messageLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    messageLabel->setOpenExternalLinks(true);

    const int HORIZONTAL_MARGIN = 10;
    const int MESSAGE_WIDTH_PERCENT = 80;
    int availableWidth = chatScrollArea->viewport()->width() - 2 * HORIZONTAL_MARGIN;
    int maxLabelWidth = qMax(50, static_cast<int>(availableWidth * (MESSAGE_WIDTH_PERCENT / 100.0)));

    QString formattedText = formatMessageText(text.toHtmlEscaped(), maxLabelWidth - 24);

    // Исправленный HTML - убрали CSS переменную
    QString messageHtml = QString(
                              "<div style='word-break: break-word; white-space: pre-wrap;'>"
                              "<b>%1</b><br>%2<br>"
                              "<small style='color: #AAAAAA;'><i>%3</i></small>"
                              "</div>"
                              ).arg(username, formattedText, time.toString("hh:mm"));

    messageLabel->setText(messageHtml);
    messageLabel->setMaximumWidth(maxLabelWidth);

    QWidget *rowWidget = new QWidget();
    rowWidget->setObjectName("messageRowWidget");
    QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(HORIZONTAL_MARGIN, 2, HORIZONTAL_MARGIN, 2);
    rowLayout->setSpacing(6);

    if (isOwnMessage) {
        rowLayout->addStretch(1);
        rowLayout->addWidget(messageLabel);
        rowWidget->setProperty("messageAlignment", "right");
    } else {
        rowLayout->addWidget(messageLabel);
        rowWidget->setProperty("messageAlignment", "left");
        rowLayout->addStretch(1);
    }

    messagesLayout->insertWidget(qMax(0, messagesLayout->count() - 1), rowWidget);
    scrollToBottom();
}

void MainWindowCodeEditor::on_actionChangeTheme_triggered()
{
    m_isDarkTheme = !m_isDarkTheme;
    applyCurrentTheme();
    updateChatButtonIcon();
}

void MainWindowCodeEditor::updateChatButtonIcon() {
    if (m_isDarkTheme) {
        m_chatButton->setIcon(QIcon(":/styles/chat_light.png"));
    } else {
        m_chatButton->setIcon(QIcon(":/styles/chat_dark.png"));
    }
    m_chatButton->setIconSize(QSize(24, 24));
}

void MainWindowCodeEditor::on_actionToDoList_triggered()
{
    TodoListWidget *todoWidget = new TodoListWidget(nullptr);
    todoWidget->setAttribute(Qt::WA_DeleteOnClose);
    todoWidget->show();

}

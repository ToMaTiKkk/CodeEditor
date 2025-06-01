<!--
CodeEditor - Documentation
Copyright (C) 2025 ToMaTiKkk
SPDX-License-Identifier: GPL-3.0-or-later
-->

1.  **Общее описание класса**
2.  **Ключевые компоненты и их инициализация (методы `setup...`)**
3.  **Основные функциональные блоки**
    *   Работа с файлами и проектом
    *   Редактор кода и его возможности (LSP, поиск, нумерация строк)
    *   Сетевое взаимодействие и совместная работа (сессии, синхронизация)
    *   Чат
    *   Терминал
    *   Управление пользователями и их статусами (мьют, админ)
    *   Темы оформления
    *   Менеджер задач (ToDo-List)
4.  **Обработка событий**
5.  **Деструктор и закрытие окна**

## Документация для `MainWindowCodeEditor`

### 1. Общее описание класса
 * MainWindowCodeEditor является центральным классом UI, который управляет всеми основными
 * компонентами приложения: редактором кода, панелью файловой системы, чатом,
 * встроенным терминалом, а также обрабатывает взаимодействие с пользователем,
 * сетевые подключения для совместной работы и интеграцию с Language Server Protocol (LSP).
 *
 * Ключевые обязанности класса:
 * - Инициализация и компоновка всех виджетов пользовательского интерфейса.
 * - Обработка действий пользователя (открытие/сохранение файлов, создание/присоединение к сессиям).
 * - Управление WebSocket-соединением для синхронизации данных с сервером и другими клиентами.
 * - Интеграция с LSP для предоставления функций автодополнения, диагностики ошибок, перехода к определению и т.д.
 * - Отображение и управление списком пользователей в сессии, включая их курсоры и подсветку строк.
 * - Реализация чата для общения между участниками сессии.
 * - Предоставление встроенного терминала для выполнения команд.
 * - Управление темами оформления (светлая/темная).
 * - Обработка событий закрытия окна с возможностью сохранения изменений.
 
### 2. Ключевые компоненты и их инициализация

Конструктор `MainWindowCodeEditor` последовательно вызывает ряд `setup...` методов для инициализации различных частей приложения:

MainWindowCodeEditor::MainWindowCodeEditor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowCodeEditor)
    // ... (инициализация членов)
{
    ui->setupUi(this); // Загрузка UI из Qt Designer

    setupMainWindow();     // Базовая настройка окна (заголовок, шрифт)
    setupCodeEditorArea(); // Настройка области редактора кода, нумерации строк, панели поиска
    setupLsp();            // Первоначальная настройка LSP-серверов
    setupLspCompletionAndHover(); // Настройка виджетов для автодополнения и всплывающих подсказок LSP
    setupChatWidget();     // Инициализация виджета чата
    setupUserFeatures();   // Настройка меню пользователей, таймера мьюта
    setupMenuBarActions(); // Подключение сигналов от действий в меню
    setupFileSystemView(); // Настройка дерева файлов
    setupNetwork();        // Инициализация WebSocket-клиента и ID клиента
    setupThemeAndNick();   // Применение темы, запрос никнейма, кнопки запуска кода и чата
    setupTerminalArea();   // Настройка области встроенного терминала   
}

**Разбор `setup...` методов:**

*   `void MainWindowCodeEditor::setupMainWindow()`
    *   Устанавливает заголовок окна "CodeEdit".
    *   Устанавливает глобальный шрифт "Fira Code", 12pt для всего приложения.

*   `void MainWindowCodeEditor::setupCodeEditorArea()`
    *   Создает кастомный виджет редактора `m_codeEditor` (наследник `QPlainTextEdit`).
    *   Настраивает редактор (шрифт, обработка табуляции, фокус, отслеживание мыши, перенос строк, скроллбары).
    *   Создает и настраивает `lineNumberArea` для отображения номеров строк рядом с редактором.
    *   Создает и настраивает панель поиска (`m_findPanel`) с полем ввода и кнопками "Следующий", "Предыдущий", "Закрыть".
    *   Компонует редактор, нумерацию строк и панель поиска в единый виджет и заменяет им плейсхолдер `ui->codeEditor` из дизайнера.
    *   Подключает сигналы для синхронизации нумерации строк с изменениями в редакторе.
    *   Инициализирует `CppHighlighter` для подсветки синтаксиса C++.
    *   Настраивает виджеты и индикаторы для LSP в статус-баре (статус LSP, количество ошибок/предупреждений).
    *   Устанавливает горячие клавиши для навигации по ошибкам (F2, Shift+F2) и для поиска.

*   `void MainWindowCodeEditor::setupLsp()`
    *   Осуществляет первоначальный поиск и настройку LSP-серверов для поддерживаемых языков.
    *   Считывает сохраненные пути к LSP-серверам из `QSettings`.
    *   Если путь не найден или невалиден, пытается автоматически найти исполняемый файл LSP-сервера в системе (используя `g_defaultLspExecutables` и `findFirstExecutable`).
    *   Сохраняет найденные/указанные пути в `QSettings`.
    *   Загружает предпочитаемый язык (по умолчанию "cpp") и вызывает `createAndStartLsp` для его запуска.

*   `void MainWindowCodeEditor::setupLspCompletionAndHover()`
    *   Инициализирует `m_completionWidget` (кастомный виджет для отображения предложений автодополнения).
    *   Инициализирует `m_diagnosticTooltip` (кастомный виджет для отображения сообщений диагностики LSP при наведении).
    *   Инициализирует `m_hoverTimer` для задержки перед отображением `hover` информации или тултипа диагностики.

*   `void MainWindowCodeEditor::setupChatWidget()`
    *   Создает и настраивает виджет чата, включая область прокрутки для сообщений (`chatScrollArea`), контейнер для списка сообщений (`messageListWidget` с `messagesLayout`), поле ввода сообщения (`chatInput`) и кнопку отправки.
    *   Скрывает контейнер чата `ui->horizontalWidget_2` по умолчанию.
    *   Подключает сигналы для отправки сообщений.

*   `void MainWindowCodeEditor::setupUserFeatures()`
    *   Инициализирует `m_userListMenu` (выпадающее меню для отображения списка пользователей в сессии).
    *   Инициализирует `m_muteTimer` для обновления отображения оставшегося времени мьюта.

*   `void MainWindowCodeEditor::setupMenuBarActions()`
    *   Централизованно подключает сигналы `triggered` от `QAction` (пунктов меню) к соответствующим слотам-обработчикам (например, `onNewFileClicked`, `onCreateSession`, `onLspSettings` и т.д.).
    *   Устанавливает начальную видимость некоторых пунктов меню, связанных с сессиями.

*   `void MainWindowCodeEditor::setupFileSystemView()`
    *   Инициализирует `QFileSystemModel` для отображения файловой системы.
    *   Устанавливает модель для `ui->fileSystemTreeView`.
    *   Скрывает ненужные колонки в дереве файлов (размер, дата изменения и т.д.).
    *   Отключает стандартные скроллбары для дерева.
    *   Подключает сигнал `doubleClicked` от дерева файлов к слоту `onFileSystemTreeViewDoubleClicked` для открытия файлов.

*   `void MainWindowCodeEditor::setupNetwork()`
    *   Генерирует уникальный идентификатор клиента (`m_clientId`) с помощью `QUuid::createUuid()`.

*   `void MainWindowCodeEditor::setupThemeAndNick()`
    *   Применяет текущую тему оформления (светлую/темную) с помощью `applyCurrentTheme()`.
    *   Создает кнопки "Запустить код" (`m_runButton`) и "Открыть/закрыть чат" (`m_chatButton`) и размещает их в правом верхнем углу менюбара.
    *   Подключает сигналы от этих кнопок к соответствующим действиям.
    *   Запрашивает у пользователя никнейм (`m_username`) с помощью `QInputDialog`. Если пользователь не вводит ник, генерируется случайный.

*   `void MainWindowCodeEditor::setupTerminalArea()`
    *   Создает виджет терминала `m_terminalWidget` (обертка `TerminalWidget`).
    *   Интегрирует терминал в главный интерфейс с помощью `QSplitter`, размещая его под основным сплиттером, содержащим редактор и файловый менеджер.
    *   Устанавливает начальную видимость терминала (`m_isTerminalVisible`).

### 3. Основные функциональные блоки

#### 3.1. Работа с файлами и проектом

*   **Открытие файла:**
    *   `onOpenFileClicked()`: Диалог выбора файла, чтение, отображение в редакторе, уведомление LSP (`notifyDidOpen`), отправка содержимого на сервер.
    *   `onFileSystemTreeViewDoubleClicked()`: Аналогично при двойном клике в дереве файлов.
*   **Сохранение файла:**
    *   `onSaveFileClicked()`: Сохраняет текущий файл. Если имя не задано, вызывает `onSaveAsFileClicked()`.
    *   `onSaveAsFileClicked()`: Диалог сохранения, запись файла, обновление `currentFilePath`, уведомление LSP.
*   **Новый файл:**
    *   `onNewFileClicked()`: Очищает редактор, сбрасывает пути, уведомляет LSP (`notifyDidClose`), отправляет пустое содержимое на сервер.
*   **Открытие папки (проекта):**
    *   `onOpenFolderClicked()`: Диалог выбора папки, обновление корневого пути в `fileSystemModel` и `ui->fileSystemTreeView`, перезапуск LSP-сервера для новой корневой папки проекта.
*   **Проверка перед закрытием/сменой файла:**
    *   `bool MainWindowCodeEditor::maybeSave()`: Если документ изменен, предлагает пользователю сохранить изменения, отменить действие или продолжить без сохранения.

#### 3.2. Редактор кода и его возможности

*   **Основной редактор:** `m_codeEditor` (экземпляр `CodePlainTextEdit`).
*   **Нумерация строк и маркеры диагностики (`LineNumberArea`):**
    *   Рядом с редактором кода `m_codeEditor` отображается кастомный виджет `LineNumberArea` (экземпляр `lineNumberArea` в `MainWindowCodeEditor`), который служит для отображения номеров строк и индикаторов диагностики (ошибок/предупреждений от LSP).
    *   **Инициализация:**
        *   `LineNumberArea` создается в `MainWindowCodeEditor::setupCodeEditorArea()`, и `m_codeEditor` передается ему в качестве родителя, что интегрирует область нумерации непосредственно в виджет редактора.
        *   В конструкторе `LineNumberArea` вызывается `updateLineNumberAreaWidth()` для начального расчета и установки ширины области.
        *   Включается отслеживание событий мыши (`setMouseTracking(true)`) для отображения всплывающих подсказок с диагностикой.
        *   Подключаются необходимые сигналы от `m_codeEditor` и его документа для синхронизации и обновления:
            *   `QPlainTextEdit::cursorPositionChanged`: для обновления подсветки текущей строки в области нумерации.
            *   `QTextDocument::contentsChange`: для обновления области нумерации (например, подсветки текущей строки) при вставке или удалении строк в редакторе.
            *   `QScrollBar::valueChanged` (от вертикального скроллбара редактора): для корректной перерисовки номеров строк при прокрутке видимой области редактора.
            *   В `MainWindowCodeEditor` также подключаются `QPlainTextEdit::updateRequest` к `MainWindowCodeEditor::updateLineNumberArea` и `QPlainTextEdit::blockCountChanged` к `MainWindowCodeEditor::updateLineNumberAreaWidth` для корректного обновления области номеров строк.
    *   **Расчет ширины области:**
        *   Ширина `LineNumberArea` рассчитывается динамически на основе максимального количества строк в редакторе.
        *   Вспомогательная функция `calculateDigits(int maxNumber)` определяет количество цифр, необходимое для отображения самого большого номера строки.
        *   Метод `LineNumberArea::calculateWidth(int digits)` вычисляет итоговую ширину, учитывая:
            *   Ширину текста для отображения `digits` самых широких цифр (например, '999').
            *   Внутренние отступы (`padding`).
            *   Ширину области для маркеров диагностики (`m_markerAreaWidth`). *Примечание: `m_markerAreaWidth` — это член класса `LineNumberArea`, значение которого должно быть установлено (например, через сеттер или иметь значение по умолчанию в объявлении класса) для корректного отображения маркеров. В предоставленном коде `LineNumberArea.cpp` его явной инициализации нет, предполагается, что он инициализируется в заголовочном файле или извне.*
        *   Метод `LineNumberArea::updateLineNumberAreaWidth()` вызывается для обновления ширины области, если количество строк (и, следовательно, количество цифр в номерах) изменилось. Установлено минимальное количество разрядов (3) для расчета ширины, чтобы область не была слишком узкой при малом количестве строк.
    *   **Отрисовка (`LineNumberArea::paintEvent`):**
        *   Фон `LineNumberArea` заливается цветом, соответствующим текущей теме приложения (светлой/темной), получаемым из `palette().color(QPalette::Window)`.
        *   Справа от номеров строк рисуется вертикальная линия-разделитель, цвет которой также зависит от темы.
        *   Итеративно проходятся все видимые блоки (строки) текста в `m_codeEditor`.
        *   Для каждой видимой строки:
            *   **Маркеры диагностики:** Если для строки имеются данные диагностики (переданные через `LineNumberArea::setDiagnotics()`), слева от номера строки рисуется цветной прямоугольный маркер. Цвет маркера зависит от уровня серьезности диагностики: красный для ошибок (severity 1), оранжевый для предупреждений (severity 2), синий для других типов. Ширина этого маркера определяется значением `m_markerAreaWidth`.
            *   **Номер строки:** Отображается номер строки (начиная с 1).
            *   **Подсветка текущей строки:** Номер строки, на которой в данный момент находится курсор в `m_codeEditor`, выделяется специальным цветом текста (синий для светлой темы, маджента для темной). Остальные номера строк отображаются стандартным цветом текста для текущей темы.
    *   **Отображение диагностических сообщений (`LineNumberArea::mouseMoveEvent`):**
        *   При наведении курсора мыши на область `LineNumberArea`:
            *   Определяется номер строки, над которой находится курсор.
            *   Если для этой строки имеются сообщения диагностики (из `m_diagnosticsMessage`, установленные через `setDiagnotics()`), то с помощью `QToolTip::showText()` отображается всплывающая подсказка с текстом этих сообщений.
            *   Если сообщений нет, тултип скрывается.
    *   **Обновление из `MainWindowCodeEditor`:**
        *   Метод `LineNumberArea::setDiagnotics(diagnostics, diagnosticsMessage)` вызывается из `MainWindowCodeEditor::updateDiagnosticsView()` для передачи актуальной информации о диагностике.
        *   Метод `LineNumberArea::updateLineNumberArea(rect, dy)` вызывается из `MainWindowCodeEditor::updateLineNumberArea()` (слот, подключенный к `m_codeEditor->updateRequest`) для частичного обновления (прокрутки или перерисовки области) `LineNumberArea` при изменениях в редакторе.
*   **Подсветка синтаксиса:** `CppHighlighter` (для C++).
*   **Основной редактор:** `m_codeEditor` (экземпляр `CodePlainTextEdit`).
*   **Нумерация строк:** `lineNumberArea` синхронизируется с редактором.
*   **Подсветка синтаксиса:** `CppHighlighter` (для C++).
*   **Поиск текста:**
    *   `m_findPanel` для UI.
    *   `showFindPanel()`: Показывает/скрывает панель поиска (Ctrl+F).
    *   `findNext()`, `findPrevious()`: Осуществляют поиск вперед/назад.
    *   `updateFindHighlights()`: Подсвечивает все вхождения искомого текста.
*   **Интеграция с LSP:**






*   **Интеграция с LSP (`LspManager`):**
    *   За взаимодействие с Language Server Protocol (LSP) отвечает класс `LspManager`. Он управляет запуском/остановкой процесса LSP-сервера, отправкой запросов и уведомлений, а также парсингом ответов и уведомлений от сервера.
    *   Экземпляр `LspManager` ( `m_lspManager` в `MainWindowCodeEditor`) создается при необходимости для конкретного языка (например, при открытии файла или смене языка).

    *   **3.2.1. Инициализация и управление процессом LSP-сервера (`LspManager`)**
        *   **Конструктор `LspManager(QString serverExecutablePath, QObject *parent)`:**
            *   Принимает путь к исполняемому файлу LSP-сервера (`serverExecutablePath`).
        *   **Деструктор `~LspManager()`:**
            *   Вызывает `stopServer()` для корректного завершения работы LSP-сервера.
        *   **Запуск сервера (`bool LspManager::startServer(languageId, projectRootPath, arguments)`):**
            *   Проверяет, не запущен ли уже сервер.
            *   Проверяет, задан ли путь к исполняемому файлу.
            *   Сохраняет `languageId` и преобразует `projectRootPath` в формат URI (`file:///...`), который требуется LSP (`m_rootUri`).
            *   Создает экземпляр `QProcess` ( `m_lspProcess`) для управления внешним процессом LSP-сервера.
            *   Подключает сигналы от `m_lspProcess`:
                *   `readyReadStandardOutput` к `onReadyReadStandardOutput` (для чтения JSON-RPC сообщений от сервера).
                *   `readyReadStandardError` к `onReadyReadStandardError` (для чтения сообщений об ошибках сервера).
                *   `finished` к `onProcessFinished` (когда процесс сервера завершается).
                *   `errorOccurred` к `onProcessError` (при ошибках запуска или работы процесса).
            *   Устанавливает программу и аргументы для `m_lspProcess`.
            *   Запускает процесс (`m_lspProcess->start()`) и ожидает его запуска до 5 секунд. При неудаче эмитирует сигнал `serverError`.
            *   Если сервер успешно запущен, отправляет ему инициализационное сообщение `initialize`.
                *   **Сообщение `initialize`:**
                    *   `jsonrpc`: "2.0"
                    *   `id`: Уникальный инкрементируемый ID запроса (`m_requestId`).
                    *   `method`: "initialize"
                    *   `params`:
                        *   `processId`: PID текущего приложения-редактора.
                        *   `rootUri`: URI корневой папки проекта.
                        *   `capabilities`: Объект, описывающий возможности клиента (редактора):
                            *   `window/showMessage`: Поддержка отображения сообщений от сервера.
                            *   `textDocument/synchronization`: Уведомления `didSave` (полный текст при изменении `SyncKind.Full`).
                            *   `textDocument/completion`: Базовая поддержка автодополнения (включая формат документации plaintext/markdown, `contextSupport`).
                            *   `textDocument/hover`: Базовая поддержка hover-подсказок.
                        *   `trace`: "off" (отладка выключена).
            *   Возвращает `true` при успешном запуске и отправке `initialize`, иначе `false`.
        *   **Остановка сервера (`void LspManager::stopServer()`):**
            *   Если процесс сервера существует и запущен:
                *   Отправляет серверу сообщение `shutdown` для корректного завершения.
                *   Ожидает ответа на `shutdown`. При получении ответа отправляет уведомление `exit`. (Текущая реализация отправляет `exit` сразу после `shutdown` *в `handleInitializeResult` или при прямом вызове `sendMessage` для `exit`* - здесь небольшое расхождение с комментарием `TODO` в коде, который предлагает дожидаться ответа на `shutdown` перед отправкой `exit`. В `handleShutdownResponse` теперь отправляется `exit`).
                *   Ожидает завершения процесса до 5 секунд. Если процесс не завершился, пытается его терминировать (`terminate()`) и, если это не помогло, принудительно убить (`kill()`).
            *   Очищает указатель `m_lspProcess`.
        *   **Проверка готовности (`bool LspManager::isReady() const`):**
            *   Возвращает `m_isServerReady` (флаг, устанавливаемый в `true` после успешной инициализации).
        *   **Обработчики событий процесса:**
            *   `onReadyReadStandardOutput()`: Читает данные из `stdout` процесса и передает их в `processIncomingData()`.
            *   `onReadyReadStandardError()`: Читает данные из `stderr` процесса и выводит их в `qWarning`.
            *   `onProcessFinished(exitCode, exitStatus)`: Вызывается при завершении процесса LSP. Устанавливает `m_isServerReady = false`, удаляет `m_lspProcess` и эмитирует сигнал `serverStopped()`.
            *   `onProcessError(error)`: Вызывается при ошибках процесса. Выводит критическое сообщение, устанавливает `m_isServerReady = false`, эмитирует `serverError()` и удаляет `m_lspProcess`.

    *   **3.2.2. Отправка и прием JSON-RPC сообщений (`LspManager`)**
        *   **Отправка сообщения (`void LspManager::sendMessage(const QJsonObject& message)`):**
            *   Проверяет, запущен ли процесс LSP.
            *   Конвертирует `QJsonObject` в компактный JSON-строку (`QByteArray`).
            *   Формирует заголовок `Content-Length: <size>\r\n\r\n`.
            *   Записывает заголовок и JSON-сообщение в `stdin` процесса LSP-сервера.
            *   Если сообщение является запросом (содержит `id` и `method`), сохраняет его `id` и `method` в `m_pendingRequests` для последующей привязки ответа.
        *   **Обработка входящих данных (`void LspManager::processIncomingData(const QByteArray& data)`):**
            *   Добавляет полученные данные в внутренний буфер `m_buffer`.
            *   В цикле пытается извлечь полные JSON-RPC сообщения из буфера:
                *   Ищет разделитель заголовка `\r\n\r\n`.
                *   Извлекает `Content-Length` из заголовка.
                *   Если `Content-Length` валиден и в буфере достаточно данных для всего сообщения (заголовок + тело), извлекает тело JSON-сообщения.
                *   Удаляет обработанное сообщение из буфера.
                *   Передает извлеченное JSON-тело в `parseMessage()`.
        *   **Разбор JSON-сообщения (`void LspManager::parseMessage(const QByteArray& jsonContent)`):**
            *   Парсит `QByteArray` в `QJsonDocument`, затем в `QJsonObject`.
            *   Определяет тип сообщения (ответ на запрос, уведомление от сервера, запрос от сервера):
                *   **Ответ на запрос (содержит `id` и `result` или `error`):**
                    *   Извлекает `id` и соответствующий `method` из `m_pendingRequests`.
                    *   Если есть `result`, вызывает соответствующий `handle...Result()` метод в зависимости от `method` (например, `handleInitializeResult`, `handleCompletionResult`).
                    *   Если есть `error`, выводит предупреждение.
                *   **Уведомление или запрос от сервера (содержит `method`):**
                    *   Извлекает `method` и `params`.
                    *   Вызывает соответствующий `handle...()` метод (например, `handlePublishDiagnostics`, `handleWindowShowMessage`).
                    *   Игнорирует некоторые уведомления, такие как `client/registerCapability` и `$/progress`.
                *   Если тип сообщения не определен, выводит предупреждение.

    *   **3.2.3. Обработчики ответов и уведомлений от LSP-сервера (`LspManager`)**
        *   **`handleInitializeResult(const QJsonObject& result)`:**
            *   Вызывается после получения ответа на `initialize`.
            *   Отправляет уведомление `initialized` серверу, чтобы подтвердить готовность клиента.
            *   Устанавливает `m_isServerReady = true`.
            *   Эмитирует сигнал `serverReady()`.
        *   **`handlePublishDiagnostics(const QJsonObject& params)`:**
            *   Извлекает `uri` файла и массив `diagnostics` из параметров.
            *   Для каждой диагностики в массиве:
                *   Парсит `range` (start line/char, end line/char), `message`, `severity`.
                *   Создает объект `LspDiagnostic`.
            *   Собирает список `LspDiagnostic` и эмитирует сигнал `diagnosticsReceived(fileUri, diagnosticsList)`.
        *   **`handleCompletionResult(const QJsonValue& resultValue)`:**
            *   Обрабатывает ответ на запрос автодополнения. Результат может быть объектом `CompletionList` (с полем `items`) или массивом `CompletionItem`.
            *   Для каждого элемента автодополнения:
                *   Парсит `label`, `insertText`, `detail`, `documentation` (может быть строкой или объектом `MarkupContent`), `kind`.
                *   Создает объект `LspCompletionItem`.
            *   Собирает список `LspCompletionItem` и эмитирует сигнал `completionReceived(completionList)`.
        *   **`handleHoverResult(const QJsonObject& result)`:**
            *   Обрабатывает ответ на запрос hover-подсказки.
            *   Парсит поле `contents` (может быть строкой, `MarkupContent` объектом или массивом `MarkedString`).
            *   Создает объект `LspHoverInfo`.
            *   Эмитирует сигнал `hoverReceived(info)`.
        *   **`handleDefinitionResult(const QJsonObject& resultOrArray)`:**
            *   Обрабатывает ответ на запрос перехода к определению. Результат может быть `null`, одним объектом `Location` или массивом `Location`.
            *   Для каждого `Location`:
                *   Парсит `uri`, `range` (start line/char).
                *   Создает объект `LspDefinitionLocation`.
            *   Собирает список `LspDefinitionLocation` и эмитирует сигнал `definitionReceived(locations)`.
        *   **Ответ на `shutdown`:**
            *   При получении ответа на запрос `shutdown`, отправляет уведомление `exit` серверу для его полного завершения.

    *   **3.2.4. Публичные методы для взаимодействия с LSP-сервером (вызываются из `MainWindowCodeEditor`)**
        *   **Уведомления серверу (Notifications):**
            *   `notifyDidOpen(fileUri, text, version)`: Отправляет `textDocument/didOpen`.
            *   `notifyDidChange(fileUri, text, version)`: Отправляет `textDocument/didChange` (с полным текстом файла).
            *   `notifyDidClose(fileUri)`: Отправляет `textDocument/didClose`.
        *   **Запросы к серверу (Requests):**
            *   `requestCompletion(fileUri, line, character, triggerKind)`: Отправляет `textDocument/completion`.
            *   `requestHover(fileUri, line, character)`: Отправляет `textDocument/hover`.
            *   `requestDefinition(fileUri, line, character)`: Отправляет `textDocument/definition`.
        *   Все запросы получают уникальный `m_requestId`, который сохраняется в `m_pendingRequests` для сопоставления с ответом.

    *   **3.2.5. Утилиты для конвертации позиций**
        *   **`QPoint LspManager::editorPosToLspPos(QTextDocument *doc, int editorPos)`:**
            *   Конвертирует абсолютную позицию символа в `QTextDocument` (`editorPos`) в LSP-формат (номер строки и номер символа в строке).
            *   Использует `doc->findBlock(editorPos)` для нахождения блока (строки) и `block.blockNumber()`, `editorPos - block.position()` для вычисления координат.
        *   **`int LspManager::lspPosToEditorPos(QTextDocument *doc, int line, int character)`:**
            *   Конвертирует LSP-позицию (строка, символ) в абсолютную позицию символа в `QTextDocument`.
            *   Использует `doc->findBlockByNumber(line)` и `block.position() + character`.
            *   *Примечание: текущая реализация не учитывает ширину табов при конвертации, что может приводить к неточностям, если в коде используются табы.*

    *   **Сигналы, эмитируемые `LspManager` (для `MainWindowCodeEditor`):**
        *   `serverReady()`: Сервер инициализирован и готов к работе.
        *   `serverStopped()`: Сервер остановлен.
        *   `serverError(QString message)`: Произошла ошибка при запуске или работе сервера.
        *   `diagnosticsReceived(QString fileUri, QList<LspDiagnostic> diagnostics)`: Получены диагностики для файла.
        *   `completionReceived(QList<LspCompletionItem> items)`: Получен список элементов автодополнения.
        *   `hoverReceived(LspHoverInfo info)`: Получена информация для hover-подсказки.
        *   `definitionReceived(QList<LspDefinitionLocation> locations)`: Получены местоположения определения.

    *   **Структуры данных для LSP (предположительно определены в `lspmanager.h` или общем заголовочном файле):**
        *   `LspDiagnostic`: Содержит `message`, `severity`, `startLine`, `startChar`, `endLine`, `endChar`.
        *   `LspCompletionItem`: Содержит `label`, `insertText`, `detail`, `documentation`, `kind`.
        *   `LspHoverInfo`: Содержит `contents` (текст подсказки).
        *   `LspDefinitionLocation`: Содержит `fileUri`, `line`, `character`.

    *   **Менеджер LSP:** `m_lspManager` (экземпляр `LspManager`).
    *   **Инициализация и запуск:** `setupLsp()`, `createAndStartLsp()`, `ensureLspForLanguage()`. Последняя функция проверяет наличие LSP для языка, ищет его автоматически, если нужно, и предлагает настроить вручную, если не найден.
    *  
    *   **Обработка ответов от сервера (слоты `onLsp...`):**
        *   `onLspServerReady()`: Сервер готов, отправляет `didOpen` для текущего файла.
        *   `onLspServerStopped()`: Сервер остановлен.
        *   `onLspServerError()`: Обработка ошибок сервера.
        *   `onLspDiagnosticsReceived()`: Получение диагностик (ошибки, предупреждения), обновление `m_diagnostics` и вызов `updateDiagnosticsView()`.
        *   `onLspCompletionReceived()`: Получение списка автодополнений, отображение в `m_completionWidget`.
        *   `onLspHoverReceived()`: Получение информации для всплывающей подсказки, отображение через `QToolTip`.
        *   `onLspDefinitionReceived()`: Получение местоположения определения, переход к нему в редакторе (если в текущем файле).
    *   **UI для LSP:**
        *   `m_completionWidget`: Виджет для отображения вариантов автодополнения. Управляется через `eventFilter` для навигации.
        *   `m_diagnosticTooltip`: Кастомный тултип для отображения диагностик.
        *   `updateDiagnosticsView()`: Обновляет подсветку ошибок/предупреждений в редакторе (`setExtraSelections`) и на `lineNumberArea`.
        *   `m_lspStatusLabel`, `m_diagnosticsStatusBtn`: Индикаторы в статус-баре.
    *   **Настройки LSP:**
        *   `onLspSettings()`: Открывает диалог `LspSettingsDialog` для настройки путей к LSP-серверам для разных языков.

### 3.3. Сетевое взаимодействие и совместная работа

Сетевое взаимодействие в приложении реализовано с использованием протокола **WebSocket**. Клиент подключается к серверу, адрес которого в текущей версии жестко задан: `ws://YOUR_WEBSOCKET_HOST:YOUR_WEBSOCKET_PORT`.

**Ключевые идентификаторы, используемые в сетевом обмене:**

*   `m_clientId` (тип `QString`): Уникальный UUID, генерируемый на стороне клиента при запуске (`setupNetwork()`) и используемый для идентификации клиента на сервере.
*   `m_username` (тип `QString`): Никнейм пользователя, запрашиваемый при запуске (`setupThemeAndNick()`).
*   `m_sessionId` (тип `QString`): Идентификатор текущей сессии. Может быть "NEW" при создании новой сессии, либо UUID существующей сессии.
*   `m_sessionPassword` (тип `QString`): Пароль, введенный пользователем для создания или присоединения к сессии.

**Подключение и отключение:**

*   `connectToServer()`: Инициирует WebSocket-соединение с сервером.
*   `onConnected()`: Слот, вызываемый при успешном подключении. Отправляет на сервер сообщение `create_session` или `join_session`.
*   `disconnectFromServer()`: Закрывает WebSocket-соединение. Если клиент был в сессии, перед закрытием он *не* отправляет явное сообщение о выходе (это может обрабатываться сервером по факту разрыва соединения или через `onLeaveSession()`).
*   `onDisconnected()`: Слот, вызываемый при разрыве соединения. Очищает информацию о сессии и удаленных пользователях.

**Все сообщения между клиентом и сервером передаются в формате JSON.**

#### 3.3.0. Диалог параметров новой сессии (`SessionParamsWindow`)

Перед созданием новой сессии (и, соответственно, перед отправкой сообщения `create_session` на сервер), пользователю отображается модальное диалоговое окно `SessionParamsWindow` для ввода необходимых параметров.

**Назначение:**
*   `SessionParamsWindow` (наследник `QDialog`) предоставляет пользовательский интерфейс для установки пароля новой сессии и, опционально, для указания, следует ли сохранять сессию на сервере и на какой срок.

**Ключевые компоненты UI и их инициализация (в конструкторе `SessionParamsWindow::SessionParamsWindow`):**

*   **Заголовок окна:** "Параметры сессии".
*   **Модальность:** Окно является модальным (`setModal(true)`).
*   **Минимальный размер:** Установлен в 390x140 пикселей.
*   **Поле ввода пароля (`passwordEdit`):**
    *   Тип: `QLineEdit`.
    *   Плейсхолдер: "мин. 4 символа".
    *   Режим ввода: `QLineEdit::Password` (скрывает вводимые символы).
*   **Опции сохранения сессии (сгруппированы в `saveOptionsWidget` с `QHBoxLayout`):**
    *   **Метка "Сохранить сессию" (`saveSessionLabel`):** `QLabel` с текстом "Сохранить сессию".
    *   **Чекбокс сохранения сессии (`saveCheckbox`):**
        *   Тип: `QCheckBox`.
        *   По умолчанию: снят (`setChecked(false)`).
        *   При изменении состояния (`stateChanged`) вызывает слот `SessionParamsWindow::onSaveCheckboxChanged`.
    *   **Метка "на" (`daysLabel`):** `QLabel` с текстом "на". По умолчанию скрыта.
    *   **Спинбокс для выбора количества дней (`daysSpinBox`):**
        *   Тип: `QSpinBox`.
        *   Диапазон: от 1 до 7 дней.
        *   Значение по умолчанию: 1.
        *   Суффикс: " дней".
        *   По умолчанию скрыт.
*   **Кнопка "Создать" (`okButton`):**
    *   Тип: `QPushButton`.
    *   Текст: "Создать".
    *   При нажатии (`clicked`) вызывает стандартный слот `QDialog::accept`, который закрывает диалог с результатом "Принято".

**Динамическое поведение:**

*   Слот `void SessionParamsWindow::onSaveCheckboxChanged(int state)`:
    *   Вызывается при изменении состояния `saveCheckbox`.
    *   Если чекбокс отмечен (`Qt::Checked`), метка `daysLabel` и спинбокс `daysSpinBox` становятся видимыми.
    *   Если чекбокс не отмечен, `daysLabel` и `daysSpinBox` скрываются.

**Публичный интерфейс для получения данных:**

После того как пользователь заполнит поля и нажмет "Создать", `MainWindowCodeEditor` может получить введенные значения с помощью следующих методов:

*   `QString SessionParamsWindow::getPassword() const`:
    *   Возвращает текст, введенный в `passwordEdit`.
*   `bool SessionParamsWindow::shouldSaveSession() const`:
    *   Возвращает `true`, если `saveCheckbox` отмечен, иначе `false`.
*   `int SessionParamsWindow::getSaveDays() const`:
    *   Если `shouldSaveSession()` возвращает `true`, то возвращает значение из `daysSpinBox` (количество дней для сохранения сессии).
    *   В противном случае (если чекбокс не отмечен), возвращает `0`.

**Использование в `MainWindowCodeEditor`:**

1.  При инициировании создания новой сессии (например, через `onCreateSession()`), `MainWindowCodeEditor` создает экземпляр `SessionParamsWindow`.
2.  Диалог отображается пользователю с помощью `paramsDialog.exec()`.
3.  Если пользователь нажимает "Создать" (диалог возвращает `QDialog::Accepted`), `MainWindowCodeEditor` использует методы `getPassword()` и `getSaveDays()` для получения пароля и срока сохранения.
4.  Эти значения затем используются для формирования полей `password` и `days` в JSON-сообщении `create_session`, которое отправляется на сервер. (`m_sessionPassword` и `m_pendingSaveDays` в `MainWindowCodeEditor` заполняются из этого диалога).

#### 3.3.1. Сообщения, отправляемые клиентом на сервер

Клиент отправляет следующие типы сообщений:

1.  **`create_session`**
    *   **Назначение:** Запрос на создание новой сессии.
    *   **Когда отправляется:** При успешном WebSocket-подключении (`onConnected()`), если `m_sessionId` установлен в "NEW" (после вызова `onCreateSession()`).
    *   **Поля JSON:**
        *   `type` (String): "create_session"
        *   `password` (String): Пароль для новой сессии (`m_sessionPassword`).
        *   `days` (Integer, опционально): Срок сохранения сессии в днях, если был указан при создании (`m_pendingSaveDays`).
        *   `username` (String): Никнейм создателя сессии (`m_username`).
        *   `client_id` (String): Уникальный ID клиента (`m_clientId`).
    *   **Примечание:** Если при создании сессии было указано `pendingSessionSave` (сохранение сразу после создания), то это сообщение может быть частью более крупного JSON, содержащего и параметры сохранения.

2.  **`join_session`**
    *   **Назначение:** Запрос на присоединение к существующей сессии.
    *   **Когда отправляется:** При успешном WebSocket-подключении (`onConnected()`), если `m_sessionId` содержит ID существующей сессии (после вызова `onJoinSession()`).
    *   **Поля JSON:**
        *   `type` (String): "join_session"
        *   `session_id` (String): ID сессии для присоединения (`m_sessionId`).
        *   `password` (String): Пароль для сессии (`m_sessionPassword`).
        *   `username` (String): Никнейм пользователя (`m_username`).
        *   `client_id` (String): Уникальный ID клиента (`m_clientId`).

3.  **`leave_session`**
    *   **Назначение:** Уведомление сервера о выходе клиента из текущей сессии.
    *   **Когда отправляется:** При вызове `onLeaveSession()` (пользователь выбрал соответствующий пункт меню).
    *   **Поля JSON:**
        *   `type` (String): "leave_session"
        *   `client_id` (String): ID клиента, покидающего сессию (`m_clientId`).

4.  **`save_session`**
    *   **Назначение:** Запрос на сохранение текущей сессии на сервере (доступно только администратору).
    *   **Когда отправляется:** При вызове `onSaveSessionClicked()`.
    *   **Поля JSON:**
        *   `type` (String): "save_session"
        *   `client_id` (String): ID клиента (администратора), инициирующего сохранение (`m_clientId`).
        *   `session_id` (String): ID текущей сессии (`m_sessionId`).
        *   `days` (Integer): Срок сохранения сессии в днях, указанный администратором.

5.  **`file_content_update`**
    *   **Назначение:** Отправка полного содержимого файла на сервер.
    *   **Когда отправляется:**
        *   При открытии файла (`onOpenFileClicked()`, `onFileSystemTreeViewDoubleClicked()`).
        *   При создании нового файла (`onNewFileClicked()`, отправляется пустой текст).
    *   **Поля JSON:**
        *   `type` (String): "file_content_update"
        *   `text` (String): Полное текстовое содержимое файла.
        *   `client_id` (String): ID клиента, отправившего обновление (`m_clientId`).
        *   `username` (String): Никнейм пользователя (`m_username`).

6.  **`insert`**
    *   **Назначение:** Уведомление сервера о вставке текста клиентом.
    *   **Когда отправляется:** При изменении текста в редакторе (`onContentsChange()`), если были добавлены символы.
    *   **Поля JSON:**
        *   `type` (String): "insert"
        *   `client_id` (String): ID клиента, выполнившего вставку (`m_clientId`).
        *   `position` (Integer): Позиция (абсолютный индекс символа от начала документа), куда был вставлен текст.
        *   `text` (String): Вставленный текст.

7.  **`delete`**
    *   **Назначение:** Уведомление сервера об удалении текста клиентом.
    *   **Когда отправляется:** При изменении текста в редакторе (`onContentsChange()`), если были удалены символы.
    *   **Поля JSON:**
        *   `type` (String): "delete"
        *   `client_id` (String): ID клиента, выполнившего удаление (`m_clientId`).
        *   `position` (Integer): Позиция (абсолютный индекс символа от начала документа), откуда началось удаление.
        *   `count` (Integer): Количество удаленных символов.

8.  **`cursor_position_update`**
    *   **Назначение:** Уведомление сервера об изменении позиции курсора клиента.
    *   **Когда отправляется:** При изменении позиции курсора в редакторе (`onCursorPositionChanged()`).
    *   **Поля JSON:**
        *   `type` (String): "cursor_position_update"
        *   `position` (Integer): Новая позиция курсора (абсолютный индекс символа).
        *   `client_id` (String): ID клиента, чей курсор изменил позицию (`m_clientId`).

9.  **`chat_message`**
    *   **Назначение:** Отправка сообщения в чат сессии.
    *   **Когда отправляется:** При отправке сообщения из виджета чата (`sendMessage()`).
    *   **Поля JSON:**
        *   `type` (String): "chat_message"
        *   `session_id` (String): ID текущей сессии (`m_sessionId`).
        *   `client_id` (String): ID клиента-отправителя (`m_clientId`).
        *   `username` (String): Никнейм отправителя (`m_username`).
        *   `text_message` (String): Текст сообщения.

10. **`mute_client`**
    *   **Назначение:** Запрос на мьют другого клиента в сессии (доступно только администратору).
    *   **Когда отправляется:** При вызове `onMuteUnmute()`, если целевой клиент не замьючен.
    *   **Поля JSON:**
        *   `type` (String): "mute_client"
        *   `target_client_id` (String): ID клиента, которого нужно замьютить.
        *   `duration` (Integer): Длительность мьюта в секундах (0 - бессрочно).
        *   `client_id` (String): ID клиента (администратора), инициирующего мьют (`m_clientId`).
        *   `session_id` (String): ID текущей сессии (`m_sessionId`).

11. **`unmute_client`**
    *   **Назначение:** Запрос на размьют другого клиента (доступно только администратору).
    *   **Когда отправляется:** При вызове `onMuteUnmute()`, если целевой клиент замьючен.
    *   **Поля JSON:**
        *   `type` (String): "unmute_client"
        *   `target_client_id` (String): ID клиента, которого нужно размьютить.
        *   `client_id` (String): ID клиента (администратора), инициирующего размьют (`m_clientId`).
        *   `session_id` (String): ID текущей сессии (`m_sessionId`).

12. **`transfer_admin`**
    *   **Назначение:** Запрос на передачу прав администратора другому клиенту (доступно только текущему администратору).
    *   **Когда отправляется:** При вызове `onTransferAdmin()`.
    *   **Поля JSON:**
        *   `type` (String): "transfer_admin"
        *   `new_admin_id` (String): ID клиента, которому передаются права администратора.
        *   `client_id` (String): ID текущего администратора (`m_clientId`).

#### 3.3.2. Сообщения, получаемые клиентом от сервера (`onTextMessageReceived()`)

Клиент обрабатывает следующие типы сообщений от сервера:

1.  **`error`**
    *   **Назначение:** Уведомление об ошибке на стороне сервера.
    *   **Поля JSON:**
        *   `type` (String): "error"
        *   `message` (String): Текст ошибки (например, "Сессия не найдена", "Неверный пароль").
        *   `session_id` (String, опционально): ID сессии, к которой относится ошибка.
        *   `days` (Integer, опционально): Может присутствовать при ошибке сохранения сессии (-1 означает ошибку).
    *   **Действия клиента:** Отображение `QMessageBox` с ошибкой, возможное отключение от сервера, сброс состояния.

2.  **`session_info`**
    *   **Назначение:** Информация о сессии, отправляемая после успешного создания или присоединения.
    *   **Поля JSON:**
        *   `type` (String): "session_info"
        *   `session_id` (String): Актуальный ID сессии.
        *   `creator_client_id` (String): ID клиента-создателя сессии (используется для определения, является ли текущий клиент админом).
        *   `text` (String): Текущее полное содержимое файла в сессии.
        *   `cursors` (Object): JSON-объект, где ключи - это `client_id` других участников, а значения - объекты с информацией о их курсорах:
            *   `position` (Integer): Позиция курсора.
            *   `username` (String): Никнейм пользователя.
            *   `color` (String): Цвет курсора пользователя (в формате, например, "#RRGGBB").
    *   **Действия клиента:** Обновление `m_sessionId`, `m_isAdmin`, установка текста в редактор, отображение курсоров других пользователей, обновление UI (например, видимость кнопок "Сохранить сессию", "Копировать ID"). Если сессия создавалась с флагом немедленного сохранения, клиент отправит запрос `save_session`.

3.  **`user_list_update`**
    *   **Назначение:** Обновленный список пользователей в сессии.
    *   **Поля JSON:**
        *   `type` (String): "user_list_update"
        *   `users` (Array): Массив JSON-объектов, каждый из которых представляет пользователя:
            *   `client_id` (String): ID клиента.
            *   `username` (String): Никнейм пользователя.
            *   `color` (String): Цвет пользователя.
            *   `is_admin` (Boolean): Является ли пользователь администратором.
            *   `mute_end_time` (Integer/Null, опционально): Unix timestamp времени окончания мьюта или `null`/отсутствует, если не замьючен или мьют снят.
    *   **Действия клиента:** Обновление локального списка `remoteUsers` и UI списка пользователей (`updateUserListUI()`).

4.  **`user_disconnected`**
    *   **Назначение:** Уведомление об отключении пользователя от сессии.
    *   **Поля JSON:**
        *   `type` (String): "user_disconnected"
        *   `client_id` (String): ID отключившегося клиента.
        *   `username` (String): Никнейм отключившегося клиента.
    *   **Действия клиента:** Удаление курсора и подсветки строки отключившегося пользователя, удаление его из `remoteUsers`, обновление UI списка пользователей.

5.  **`file_content_update`**
    *   **Назначение:** Полное обновление содержимого файла (например, если другой пользователь открыл новый файл в сессии).
    *   **Поля JSON:**
        *   `type` (String): "file_content_update"
        *   `text` (String): Новое полное содержимое файла.
        *   *(Клиент не использует `client_id` и `username` из этого сообщения, но они могут присутствовать)*
    *   **Действия клиента:** Полная замена текста в `m_codeEditor` (с блокировкой сигналов, чтобы избежать отправки изменений обратно).

6.  **`chat_message`**
    *   **Назначение:** Новое сообщение в чате от другого пользователя.
    *   **Поля JSON:**
        *   `type` (String): "chat_message"
        *   `username` (String): Никнейм отправителя.
        *   `text_message` (String): Текст сообщения.
        *   *(Клиент не использует `session_id` и `client_id` из этого сообщения, но они могут присутствовать)*
    *   **Действия клиента:** Добавление сообщения в виджет чата (`addChatMessageWidget()`). Если окно свернуто или чат не активен, показывается системное уведомление (`QSystemTrayIcon`).

7.  **`cursor_position_update`**
    *   **Назначение:** Обновление позиции курсора другого пользователя.
    *   **Поля JSON:**
        *   `type` (String): "cursor_position_update"
        *   `client_id` (String): ID клиента, чей курсор переместился.
        *   `position` (Integer): Новая позиция курсора.
        *   `username` (String): Никнейм пользователя.
        *   `color` (String): Цвет курсора.
    *   **Действия клиента:** Если это не собственный `client_id`, создается/обновляется `CursorWidget` и `LineHighlightWidget` для данного пользователя, их геометрия обновляется (`updateRemoteWidgetGeometry()`, `updateLineHighlight()`).

8.  **`mute_notification`**
    *   **Назначение:** Уведомление текущего клиента о том, что его замьютили.
    *   **Поля JSON:**
        *   `type` (String): "mute_notification"
        *   `duration` (Integer): Длительность мьюта в секундах (0 - бессрочно).
    *   **Действия клиента:** Отображение `QMessageBox` с информацией о мьюте, вызов `updateMutedStatus()` для блокировки редактора и обновления статус-бара.

9.  **`muted_status_update`**
    *   **Назначение:** Обновление статуса мьюта для одного из пользователей в сессии (включая текущего).
    *   **Поля JSON:**
        *   `type` (String): "muted_status_update"
        *   `client_id` (String): ID клиента, чей статус мьюта изменился.
        *   `is_muted` (Boolean): `true`, если клиент замьючен, `false` - если размьючен.
        *   `mute_end_time` (Integer/Null, опционально): Unix timestamp времени окончания мьюта. `null` или отсутствие означает, что мьют снят или бессрочный (если `is_muted` true и `duration` при мьюте был 0).
    *   **Действия клиента:** Обновление состояния мьюта в `m_mutedClients` и `m_muteEndTimes`. Вызов `onMutedStatusUpdate()`, который, в свою очередь, обновляет UI (редактор, список пользователей, подсветку строк).

10. **`admin_changed`**
    *   **Назначение:** Уведомление о смене администратора сессии.
    *   **Поля JSON:**
        *   `type` (String): "admin_changed"
        *   `new_admin_id` (String): ID нового администратора сессии.
    *   **Действия клиента:** Вызов `onAdminChanged()`, который обновляет флаг `m_isAdmin`, UI (кнопку сохранения сессии, список пользователей) и выводит сообщение в статус-бар.

11. **`insert`**
    *   **Назначение:** Операция вставки текста, выполненная другим пользователем.
    *   **Поля JSON:**
        *   `type` (String): "insert"
        *   `client_id` (String): ID клиента, выполнившего вставку (игнорируется, если совпадает с `m_clientId`).
        *   `position` (Integer): Позиция вставки.
        *   `text` (String): Вставленный текст.
    *   **Действия клиента:** Вставка текста в `m_codeEditor` в указанную позицию (с блокировкой сигналов).

12. **`delete`**
    *   **Назначение:** Операция удаления текста, выполненная другим пользователем.
    *   **Поля JSON:**
        *   `type` (String): "delete"
        *   `client_id` (String): ID клиента, выполнившего удаление (игнорируется, если совпадает с `m_clientId`).
        *   `position` (Integer): Позиция, с которой началось удаление.
        *   `count` (Integer): Количество удаленных символов.
    *   **Действия клиента:** Удаление текста из `m_codeEditor` (с блокировкой сигналов).

13. **`session_saved`**
    *   **Назначение:** Уведомление об успешном сохранении сессии на сервере.
    *   **Поля JSON:**
        *   `type` (String): "session_saved"
        *   `days` (Integer): На сколько дней сессия была сохранена.
    *   **Действия клиента:** Отображение сообщения в статус-баре.

#### 3.4. Чат

*   **UI:** Инициализируется в `setupChatWidget()`. Состоит из `chatScrollArea`, `messageListWidget`, `messagesLayout` (для отображения сообщений) и `chatInput` с кнопкой отправки.
*   **Отправка сообщений:**
    *   `sendMessage()`: Вызывается при нажатии кнопки "Отправить" или Enter в `chatInput`. Формирует JSON-сообщение `chat_message` и отправляет на сервер. Локально добавляет сообщение с пометкой "Вы".
*   **Получение сообщений:**
    *   В `onTextMessageReceived()` при `opType == "chat_message"`. Вызывает `addChatMessageWidget()` для отображения.
*   **Отображение сообщений:**
    *   `addChatMessageWidget()`: Создает кастомный `QLabel` для каждого сообщения, форматирует его (имя пользователя, текст, время, выравнивание для своих/чужих сообщений) и добавляет в `messagesLayout`.
    *   `scrollToBottom()`: Автоматически прокручивает чат вниз при добавлении нового сообщения.
*   **Уведомления:** Если окно свернуто или чат не виден, при получении нового сообщения используется `QSystemTrayIcon` для показа системного уведомления.

### 3.5. Терминал (`TerminalWidget`)

Встроенный терминал реализован с помощью класса `TerminalWidget`, который является оберткой над библиотечным компонентом `QTermWidget`. `TerminalWidget` предоставляет сконфигурированный и готовый к использованию эмулятор терминала, интегрированный в основной интерфейс приложения.

**Расположение в UI:**
Экземпляр `TerminalWidget` ( `m_terminalWidget` в `MainWindowCodeEditor`) инициализируется в методе `MainWindowCodeEditor::setupTerminalArea()`. Он интегрируется в главный интерфейс с помощью `QSplitter`, размещаясь под основным сплиттером, содержащим редактор и файловый менеджер. Начальная видимость терминала (`m_isTerminalVisible`) управляется `MainWindowCodeEditor`.

#### 3.5.1. Ключевые компоненты и инициализация (`TerminalWidget::TerminalWidget`)

Конструктор `TerminalWidget` выполняет следующую настройку:

1.  **Создание `QTermWidget`:**
    *   Создается экземпляр `term_widget = new QTermWidget(this)`. `this` (экземпляр `TerminalWidget`) становится родителем, что обеспечивает корректное управление памятью в Qt.
    *   Устанавливается отладочное имя объекта: `term_widget->setObjectName("internalTermWidget")`.

2.  **Настройка шрифта:**
    *   Для терминала устанавливается шрифт "Fira Code" размером 12pt.
    *   `termFont.setStyleHint(QFont::TypeWriter);` используется для гарантии выбора моноширинного варианта шрифта, что критически важно для корректного отображения в терминале.
    *   Шрифт применяется к `QTermWidget` через `term_widget->setTerminalFont(termFont)`.

3.  **Цветовая схема:**
    *   По умолчанию применяется темная цветовая схема, загружаемая из ресурсов: `term_widget->setColorScheme(":/styles/Dark.colorscheme");`. Эта схема может быть изменена извне через метод `applyColorScheme()`.

4.  **Компоновка:**
    *   Создается `QVBoxLayout` (`layout`), который становится основным менеджером компоновки для `TerminalWidget`.
    *   `term_widget` добавляется в этот `layout` с нулевыми отступами (`layout->setContentsMargins(0, 0, 0, 0)`).
    *   `term_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);` устанавливается для того, чтобы внутренний `QTermWidget` занимал все доступное пространство внутри `TerminalWidget`.

5.  **Подключение сигналов:**
    *   `connect(term_widget, &QTermWidget::termKeyPressed, this, &TerminalWidget::handleKeyPress);`
        *   Сигнал `termKeyPressed` от внутреннего `QTermWidget` подключается к слоту `handleKeyPress` в `TerminalWidget`. Это позволяет перехватывать и обрабатывать некоторые нажатия клавиш (например, для копирования).
    *   `connect(term_widget, &QTermWidget::urlActivated, this, &TerminalWidget::handleLinkActivation);`
        *   Сигнал `urlActivated` (когда пользователь кликает на ссылку в терминале) подключается к слоту `handleLinkActivation`. Это позволяет открывать URL в системном браузере.

#### 3.5.2. Публичный интерфейс `TerminalWidget`

Класс `TerminalWidget` предоставляет следующие публичные методы для взаимодействия с ним из `MainWindowCodeEditor`:

*   `void setInputFocus()`:
    *   Устанавливает фокус ввода на внутренний `QTermWidget` (`term_widget->setFocus()`). Это необходимо, чтобы пользователь мог сразу начать вводить команды в терминал после его отображения.

*   `void applyColorScheme(const QString &schemePath)`:
    *   Применяет к внутреннему `QTermWidget` цветовую схему, указанную путем `schemePath` к файлу `.colorscheme`. Это используется для синхронизации темы терминала с общей темой приложения (светлая/темная).

*   `void sendCommand(const QString& command)`:
    *   Отправляет текстовую команду во внутренний `QTermWidget` для исполнения.
    *   Метод автоматически добавляет символ новой строки (`\n`) в конец команды, если он отсутствует, так как большинство оболочек командной строки требуют этого для выполнения команды.
    *   Используется `term_widget->sendText(commandToSend);` для отправки.

#### 3.5.3. Обработка событий (приватные слоты)

*   `void handleKeyPress(QKeyEvent *event)`:
    *   Этот слот вызывается при каждом нажатии клавиши в `QTermWidget`.
    *   Он проверяет, соответствует ли нажатие стандартной комбинации для копирования (`event->matches(QKeySequence::Copy)`).
    *   Если да, то вызывается `term_widget->copyClipboard()` для копирования выделенного текста из терминала в буфер обмена, и событие помечается как обработанное (`event->accept()`).
    *   Для всех остальных нажатий клавиш событие помечается как не обработанное (`event->ignore()`), позволяя `QTermWidget` обработать их стандартным образом (т.е. отправить ввод в процесс оболочки).

*   `void handleLinkActivation(const QUrl &url, bool fromContextMenu)`:
    *   Слот вызывается, когда пользователь активирует URL в терминале (например, кликом мыши).
    *   Если `url` валиден, он открывается с помощью `QDesktopServices::openUrl(url)`, используя стандартное приложение системы для данного типа URL (обычно веб-браузер).
    *   Параметр `fromContextMenu` в текущей реализации не используется (`Q_UNUSED`).

#### 3.5.4. Деструктор (`TerminalWidget::~TerminalWidget`)

*   Деструктор `TerminalWidget` в основном выполняет логирование.
*   Явного удаления `term_widget` и `layout` не происходит, так как они были созданы с `TerminalWidget` в качестве родителя, и система управления памятью Qt позаботится об их удалении.

#### 3.5.5. Интеграция и использование в `MainWindowCodeEditor`

*   **Показ/скрытие:** Управляется через `QAction` `ui->actionTerminal` из `MainWindowCodeEditor`. Слот `MainWindowCodeEditor::on_actionTerminal_triggered()` обрабатывает изменение состояния этого `QAction` и вызывает `m_terminalWidget->setVisible()`, а также устанавливает фокус ввода.
*   **Компиляция и запуск кода:** Метод `MainWindowCodeEditor::compileAndRun()` формирует команду для компиляции и/или запуска текущего файла в зависимости от его расширения и отправляет эту команду в терминал через `m_terminalWidget->sendCommand()`. Если терминал скрыт, он автоматически показывается.

#### 3.6. Управление пользователями и их статусами

*   **Список пользователей:**
    *   `m_userListMenu`: `QMenu` для отображения списка участников сессии.
    *   `onShowUserList()`: Показывает это меню.
    *   `updateUserListUI()`: Обновляет содержимое меню на основе `remoteUsers`. Для каждого пользователя создается `QAction` с иконкой (цвет пользователя), именем, и статусом (админ, мьют).
    *   Для каждого пользователя в списке доступно контекстное меню с действиями (Информация, Мьют/Размьют, Передать права админа - если текущий пользователь админ).
*   **Мьют/Размьют:**
    *   `onMuteUnmute()`: Отправляет на сервер команду `mute_client` или `unmute_client`.
    *   `m_mutedClients` (QMap<QString, int>): Хранит статус мьюта для клиентов (1 - замьючен).
    *   `m_muteEndTimes` (QMap<QString, qint64>): Хранит время окончания мьюта.
    *   `updateMutedStatus()`: Обновляет состояние `m_codeEditor` (делает его `ReadOnly`, если текущий пользователь замьючен) и статус-бар.
    *   `m_muteTimer`: `QTimer` для обновления отображения оставшегося времени мьюта в статус-баре (`updateStatusBarMuteTime`) и в окне информации о пользователе (`updateMuteTimeDisplayInUserInfo`).
    *   `formatMuteTime()`: Форматирует оставшееся время мьюта в удобочитаемый вид.
*   **Передача прав администратора:**
    *   `onTransferAdmin()`: (Только для админа) Отправляет команду `transfer_admin` на сервер.
    *   `onAdminChanged()`: Обрабатывает уведомление о смене админа, обновляет `m_isAdmin` и UI.
*   **Информация о пользователе:**
    *   `showUserInfo()`: Показывает `QMessageBox` с информацией о выбранном пользователе (ник, ID, статус мьюта, админ-статус). Время мьюта обновляется динамически.

#### 3.7. Темы оформления

*   **Переключение тем:**
    *   `on_actionChangeTheme_triggered()`: Слот для пункта меню "Сменить тему". Инвертирует флаг `m_isDarkTheme` и вызывает `applyCurrentTheme()`.
    *   `applyCurrentTheme()`: Загружает соответствующий QSS-файл (`:/styles/light.qss` или `:/styles/dark.qss`) и применяет стили к приложению (`qApp`), чату, виджету автодополнения и терминалу.
    *   `updateChatButtonIcon()`: Обновляет иконку кнопки чата в зависимости от темы.

### 3.8. Менеджер задач (ToDo List)

Приложение включает в себя менеджер задач, реализованный в классе `TodoListWidget`. Он позволяет пользователям создавать, отслеживать и управлять своими задачами и заметками с возможностью указания крайних сроков.

#### 3.8.1. Запуск и отображение

*   Доступ к менеджеру задач осуществляется через пункт меню "Инструменты" -> "Список дел" (предполагается, что `ui->actionToDoList` так называется и его сигнал `triggered` подключен к слоту `MainWindowCodeEditor::on_actionToDoList_triggered()`).
*   При выборе этого пункта меню создается новый экземпляр `TodoListWidget` как отдельное, независимое окно (`new TodoListWidget(nullptr)`).
*   Виджет настроен на автоматическое удаление при закрытии (`setAttribute(Qt::WA_DeleteOnClose)`), что предотвращает утечки памяти.

#### 3.8.2. Ключевые компоненты UI (`TodoListWidget::setupUI`)

Интерфейс `TodoListWidget` состоит из следующих элементов, скомпонованных с помощью `QVBoxLayout` и `QHBoxLayout`:

*   **Панель ввода задачи:**
    *   `taskInput` ( `QLineEdit`): Поле для ввода текста новой задачи. Имеет плейсхолдер "Введите задачу...".
    *   `selectDateTimeButton` ( `QPushButton`): Кнопка с иконкой календаря ( `:/styles/calendar-clock.png`), открывающая диалог выбора даты и времени.
    *   `dateTimeInput` ( `QDateTimeEdit`): Отображает выбранную дату и время для задачи в формате "dd.MM.yyyy HH:mm". Поле доступно только для чтения, значение устанавливается через диалог. Изначально отображает текущую дату и время.
    *   `addButton` ( `QPushButton`): Кнопка "Добавить" для добавления новой задачи в список.
*   **Список задач:**
    *   `taskList` ( `QListWidget`): Основной виджет для отображения списка задач.
*   **Панель управления задачами:**
    *   `doneButton` ( `QPushButton`): Кнопка "Выполнено" для отметки выбранных задач как выполненных.
    *   `deleteButton` ( `QPushButton`): Кнопка "Удалить" для удаления выбранных задач.
    *   `unmarkButton` ( `QPushButton`): Кнопка "Не выполнено" для снятия отметки о выполнении с выбранных задач.

#### 3.8.3. Функциональность `TodoListWidget`

*   **Добавление задачи (`addTask()`):**
    *   Срабатывает при нажатии кнопки "Добавить".
    *   Считывает текст из `taskInput` и дату/время из `dateTimeInput`.
    *   Если текст задачи пуст, выводится предупреждение.
    *   Создается новый `QListWidgetItem` с текстом задачи и указанием срока выполнения (например, "Сделать отчет | Дата: 01.01.2024 12:00").
    *   Элементу списка устанавливается флаг `Qt::ItemIsEditable`, позволяющий редактировать текст задачи.
    *   Поле `taskInput` очищается.

*   **Удаление задачи (`deleteTask()`):**
    *   Срабатывает при нажатии кнопки "Удалить".
    *   Если ни одна задача не выбрана, выводится предупреждение.
    *   Запрашивается подтверждение на удаление.
    *   При подтверждении выбранные элементы удаляются из `taskList`.

*   **Отметка задачи как выполненной (`markTaskDone()`):**
    *   Срабатывает при нажатии кнопки "Выполнено".
    *   Если ни одна задача не выбрана, выводится предупреждение.
    *   Для каждой выбранной задачи, если она еще не отмечена как выполненная:
        *   Цвет текста устанавливается на белый (`QColor(255, 255, 255)`).
        *   Цвет фона устанавливается на зеленый (`QColor(80, 150, 50, 200)`).
        *   Снимается флаг `Qt::ItemIsEditable`, запрещая редактирование выполненной задачи.

*   **Снятие отметки о выполнении (`unmarkTaskDone()`):**
    *   Срабатывает при нажатии кнопки "Не выполнено".
    *   Если ни одна задача не выбрана, выводится предупреждение.
    *   Для каждой выбранной задачи, если она была отмечена как выполненная:
        *   Цвет текста и фона сбрасываются на значения по умолчанию (из палитры виджета).
        *   Восстанавливается флаг `Qt::ItemIsEditable`.
    *   После изменения статуса вызывается `saveTasksToFile()` для немедленного сохранения изменений.

*   **Редактирование задачи (`editTask()`):**
    *   Срабатывает при двойном клике на элементе списка (`taskList->itemDoubleClicked`).
    *   Если элемент существует и имеет флаг `Qt::ItemIsEditable` (т.е. не выполнен), `taskList->editItem(item)` активирует режим редактирования текста задачи.

*   **Выбор даты и времени (`showDateTimePicker()`):**
    *   Срабатывает при нажатии кнопки `selectDateTimeButton`.
    *   Создает и отображает модальный диалог `DateTimePickerDialog`.
    *   Если пользователь подтверждает выбор в диалоге, выбранная дата и время устанавливаются в `dateTimeInput`.

*   **Выбор элемента в списке (`onItemClicked()`):**
    *   В текущей реализации слот `onItemClicked` пуст и не выполняет никаких действий при простом клике на задачу.

#### 3.8.4. Сохранение и загрузка задач

*   **Путь к файлу:** Задачи сохраняются в текстовый файл. Путь к файлу в коде не инициализируется явно, он будет создан в текущей рабочей директории приложения.
*   **Загрузка задач (`loadTasksFromFile()`):**
    *   Вызывается в конструкторе `TodoListWidget`.
    *   Если файл по пути `filePath` существует, он открывается для чтения.
    *   Каждая строка файла интерпретируется как одна задача. Формат строки: `[статус] [текст задачи с датой]`.
        *   `статус`: "1" (выполнена) или "0" (не выполнена).
        *   `текст задачи с датой`: Полный текст элемента, как он отображается.
    *   Создается `QListWidgetItem` для каждой задачи. Если задача была выполнена, ей устанавливаются соответствующие цвета фона/текста и снимается флаг редактирования.

*   **Сохранение задач (`saveTasksToFile()`):**
    *   Вызывается в деструкторе `TodoListWidget` и после снятия отметки о выполнении (`unmarkTaskDone`).
    *   Файл открывается для записи (с перезаписью содержимого `QIODevice::Truncate`).
    *   Для каждой задачи в `taskList`:
        *   Определяется ее статус выполнения по цвету фона.
        *   В файл записывается строка в формате: `[статус] [текст задачи]`.
    *   Если файл не удается открыть для записи, выводится предупреждение в `qWarning`.

#### 3.8.5. Диалог выбора даты и времени (`DateTimePickerDialog`)

Класс `DateTimePickerDialog` представляет собой модальный диалог для удобного выбора даты и времени.

*   **Инициализация:**
    *   Принимает начальную дату и время (`initialDateTime`).
    *   `QCalendarWidget` для выбора даты (скрыты стандартные заголовки, дни других недель отображаются серым).
    *   `QTimeEdit` для выбора времени (формат "HH:mm").
    *   `QDialogButtonBox` с кнопками "Ok" и "Cancel".
*   **Функциональность:**
    *   `updateDateTime()`: Слот, вызываемый при изменении выбора в `QCalendarWidget` или `QTimeEdit`. Обновляет внутреннее значение `currentDateTime`.
    *   `getSelectedDateTime()`: Возвращает выбранную пользователем дату и время (`QDateTime`).

### 4. Обработка событий

*   `bool MainWindowCodeEditor::eventFilter(QObject *obj, QEvent *event)`:
    *   Перехватывает события для `m_completionWidget` (навигация клавишами, выбор элемента, Escape для скрытия, перенаправление текстового ввода в редактор).
    *   Перехватывает события для `m_codeEditor->viewport()`:
        *   `QEvent::Resize`: Обновляет подсветку строк удаленных пользователей.
        *   `QEvent::MouseMove`: Вызывает `handleEditorMouseMoveEvent()` для логики hover.
        *   `QEvent::Leave`: Скрывает тултипы и останавливает `m_hoverTimer`.
*   `void MainWindowCodeEditor::handleEditorKeyPressEvent(QKeyEvent *event)`:
    *   Обрабатывает нажатия клавиш в редакторе для специфичных шорткатов (Ctrl+Пробел для автодополнения, F12 для перехода к определению), если они не были обработаны `eventFilter`. *Примечание: в коде есть `connect(m_codeEditor, &CodePlainTextEdit::completionShortcut, ...)` и `definitionnShortcut`, так что эта функция может быть частично избыточна или для других сценариев.*
*   `void MainWindowCodeEditor::handleEditorMouseMoveEvent(QMouseEvent *event)`:
    *   Отслеживает движение мыши над редактором и запускает/перезапускает `m_hoverTimer` для отображения LSP hover информации или диагностических тултипов.
*   `void MainWindowCodeEditor::closeEvent(QCloseEvent *event)`:
    *   Перед закрытием окна вызывает `maybeSave()` для сохранения изменений.
    *   Останавливает LSP-сервер (`m_lspManager->stopServer()`).
    *   Отключается от WebSocket-сервера (`disconnectFromServer()`).

### 5. Деструктор и закрытие окна

*   `MainWindowCodeEditor::~MainWindowCodeEditor()`:
    *   Останавливает LSP-сервер, если он активен.
    *   Скрывает и удаляет иконку в трее (`m_trayIcon`), если она есть.
    *   Удаляет `ui`.
    *   Удаляет все виджеты удаленных курсоров (`remoteCursors`) и подсветок строк (`remoteLineHighlights`).
    *   Удаляет WebSocket (`socket`), подсветку синтаксиса (`highlighter`), чекбокс темы (`m_themeCheckBox` - *похоже, он не используется активно в предоставленном коде, но удаляется*), меню пользователей (`m_userListMenu`), таймер мьюта (`m_muteTimer`), метку времени мьюта (`m_muteTimeLabel` - *аналогично, может быть остатком*), область нумерации строк (`lineNumberArea`), редактор кода (`m_codeEditor`), виджет автодополнения (`m_completionWidget`).

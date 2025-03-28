#include "todolistwidget.h"
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QDateTimeEdit>
#include <QTimeEdit>
#include <QCalendarWidget>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QCloseEvent>
#include <QColor>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <QLabel>
#include <QIcon>
#include <QSize>

// --- Реализация TodoListWidget ---

TodoListWidget::TodoListWidget(QWidget *parent)
    : QWidget(parent),
      taskList(new QListWidget(this)),
      taskInput(new QLineEdit(this)),
      dateTimeInput(new QDateTimeEdit(this)),
      addButton(new QPushButton("Добавить", this)),
      selectDateTimeButton(new QPushButton(this)), // Создаем кнопку без текста
      doneButton(new QPushButton("Выполнено", this)),
      deleteButton(new QPushButton("Удалить", this))
{
    setupUI();
    loadTasksFromFile();

    // Соединения сигналов и слотов
    connect(addButton, &QPushButton::clicked, this, &TodoListWidget::addTask);
    connect(doneButton, &QPushButton::clicked, this, &TodoListWidget::markTaskDone);
    connect(deleteButton, &QPushButton::clicked, this, &TodoListWidget::deleteTask);
    connect(selectDateTimeButton, &QPushButton::clicked, this, &TodoListWidget::showDateTimePicker);
    connect(taskList, &QListWidget::itemClicked, this, &TodoListWidget::onItemClicked);
    connect(taskList, &QListWidget::itemDoubleClicked, this, &TodoListWidget::editTask);
}

TodoListWidget::~TodoListWidget() {
    saveTasksToFile();
}

void TodoListWidget::saveTasksToFile() {
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        for (int i = 0; i < taskList->count(); ++i) {
            QListWidgetItem *item = taskList->item(i);
            bool isCompleted = item->foreground().color() == QColor(255, 255, 255);
            QString taskText = item->text();
            out << (isCompleted ? "1" : "0") << " " << taskText << "\n";
        }
        file.close();
    } else {
        qWarning() << "Не удалось открыть файл для записи: " << file.errorString();
    }
}

void TodoListWidget::loadTasksFromFile() {
    QFile file(filePath);
    if (!file.exists()) return;

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            int firstSpace = line.indexOf(' ');
            if (firstSpace != -1) {
                QString statusPart = line.left(firstSpace);
                QString taskText = line.mid(firstSpace + 1);
                bool isCompleted = statusPart == "1";

                QListWidgetItem *item = new QListWidgetItem(taskText, taskList);
                item->setFlags(item->flags() | Qt::ItemIsEditable);

                if (isCompleted) {
                    item->setForeground(QColor(255, 255, 255));
                    item->setBackground(QColor(80, 150, 50, 200));
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                }
            }
        }
        file.close();
    } else {
         qWarning() << "Не удалось открыть файл для чтения: " << file.errorString();
    }
}


// Настройка UI
void TodoListWidget::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *inputLayout = new QHBoxLayout;

    taskInput->setPlaceholderText("Введите задачу...");
    dateTimeInput->setDateTime(QDateTime::currentDateTime());
    dateTimeInput->setDisplayFormat("dd.MM.yyyy HH:mm");

    // Делаем поле даты/времени read-only и убираем стрелки
    dateTimeInput->setReadOnly(true);
    dateTimeInput->setStyleSheet(R"(
        QDateTimeEdit::up-button { width: 0px; border: none; }
        QDateTimeEdit::down-button { width: 0px; border: none; }
        QDateTimeEdit[readOnly="true"] {
            background-color: #f0f0f0;
            color: #555;
        }
    )");
    dateTimeInput->setButtonSymbols(QAbstractSpinBox::NoButtons);

    // --- Настройка кнопки с иконкой из РЕСУРСОВ ---
    selectDateTimeButton->setToolTip("Выбрать дату и время");


    QIcon dateTimeIcon(":/calendar-clock.png");

    if (!dateTimeIcon.isNull()) { // Проверка, что ресурс найден и загружен
        selectDateTimeButton->setIcon(dateTimeIcon);
        selectDateTimeButton->setText(""); // Убираем текст, если иконка есть
        // Устанавливаем размер кнопки по содержимому (иконке)
        selectDateTimeButton->setFixedSize(selectDateTimeButton->sizeHint());
    } else {
        qWarning() << "Не удалось загрузить иконку из ресурсов ':/calendar-clock.png'. Используется текст.";
        selectDateTimeButton->setText("..."); // Запасной вариант текстом
        selectDateTimeButton->setFixedSize(selectDateTimeButton->sizeHint()); // Размер по тексту
    }
    // --- Конец настройки кнопки с иконкой ---

    // Добавляем виджеты в inputLayout
    inputLayout->addWidget(taskInput);
    inputLayout->addWidget(selectDateTimeButton); // Добавляем кнопку (с иконкой или текстом)
    inputLayout->addWidget(dateTimeInput);
    inputLayout->addWidget(addButton);

    mainLayout->addLayout(inputLayout);
    mainLayout->addWidget(taskList);

    // Кнопки управления списком
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(doneButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

void TodoListWidget::onItemClicked(QListWidgetItem *item) {
    // Пусто
}

void TodoListWidget::addTask() {
    QString taskText = taskInput->text().trimmed();
    QDateTime deadline = dateTimeInput->dateTime();

    if (taskText.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите название задачи!");
        return;
    }

    QString taskItemText = taskText + " | Дата: " + deadline.toString("dd.MM.yyyy HH:mm");
    QListWidgetItem *item = new QListWidgetItem(taskItemText, taskList);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    taskInput->clear();
}

void TodoListWidget::deleteTask() {
    QList<QListWidgetItem*> items = taskList->selectedItems();
    if (items.isEmpty()) {
         QMessageBox::warning(this, "Ошибка", "Выберите задачу (или задачи) для удаления!");
         return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Удаление задачи",
                                  QString("Вы уверены, что хотите удалить %1 задач(и)?").arg(items.count()),
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        // Используем qDeleteAll для безопасного удаления выбранных элементов
        qDeleteAll(taskList->selectedItems());
    }
}

void TodoListWidget::markTaskDone() {
     QList<QListWidgetItem*> items = taskList->selectedItems();
     if (items.isEmpty()) {
          QMessageBox::warning(this, "Ошибка", "Выберите задачу (или задачи) для отметки выполненной!");
          return;
     }

     foreach(QListWidgetItem *item, items) {
        // Проверяем по цвету фона, чтобы не выполнять повторно
        if (item->background().color() != QColor(80, 150, 50, 200)) {
            item->setForeground(QColor(255, 255, 255));
            item->setBackground(QColor(80, 150, 50, 200));
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        }
    }
}

void TodoListWidget::editTask(QListWidgetItem *item) {
    if (item && (item->flags() & Qt::ItemIsEditable)) {
        taskList->editItem(item);
        // Примечание: Редактируется вся строка. Если нужно редактировать только текст задачи,
        // потребуется извлечь дату, позволить редактировать только текст, а затем собрать строку обратно.
    }
}

// Слот для показа кастомного диалога выбора даты и времени
void TodoListWidget::showDateTimePicker() {
    DateTimePickerDialog dialog(dateTimeInput->dateTime(), this);
    if (dialog.exec() == QDialog::Accepted) {
        dateTimeInput->setDateTime(dialog.getSelectedDateTime());
    }
}


// --- Реализация DateTimePickerDialog ---

DateTimePickerDialog::DateTimePickerDialog(const QDateTime &initialDateTime, QWidget *parent)
    : QDialog(parent), currentDateTime(initialDateTime)
{
    setWindowTitle("Выберите дату и время");
    setModal(true);

    calendarWidget = new QCalendarWidget(this);
    calendarWidget->setSelectedDate(initialDateTime.date());

    timeEdit = new QTimeEdit(this);
    timeEdit->setTime(initialDateTime.time());
    timeEdit->setDisplayFormat("HH:mm");

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(calendarWidget, &QCalendarWidget::selectionChanged, this, &DateTimePickerDialog::updateDateTime);
    connect(timeEdit, &QTimeEdit::timeChanged, this, &DateTimePickerDialog::updateDateTime);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(calendarWidget);

    QHBoxLayout *timeLayout = new QHBoxLayout();
    timeLayout->addWidget(new QLabel("Время:", this));
    timeLayout->addWidget(timeEdit);
    timeLayout->addStretch();
    mainLayout->addLayout(timeLayout);

    mainLayout->addWidget(buttonBox);

    updateDateTime(); // Инициализируем currentDateTime
}

void DateTimePickerDialog::updateDateTime() {
    currentDateTime.setDate(calendarWidget->selectedDate());
    currentDateTime.setTime(timeEdit->time());
}

QDateTime DateTimePickerDialog::getSelectedDateTime() const {
    return currentDateTime;
}

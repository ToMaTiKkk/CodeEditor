// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

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


TodoListWidget::TodoListWidget(QWidget *parent)
    : QWidget(parent),
      taskList(new QListWidget(this)),
      taskInput(new QLineEdit(this)),
      dateTimeInput(new QDateTimeEdit(this)),
      addButton(new QPushButton("Добавить", this)),
      selectDateTimeButton(new QPushButton(this)),
      doneButton(new QPushButton("Выполнено", this)),
      unmarkButton(new QPushButton("Не выполнено", this)),
      deleteButton(new QPushButton("Удалить", this))
{
    setupUI();
    loadTasksFromFile();

    connect(addButton, &QPushButton::clicked, this, &TodoListWidget::addTask);
    connect(doneButton, &QPushButton::clicked, this, &TodoListWidget::markTaskDone);
    connect(deleteButton, &QPushButton::clicked, this, &TodoListWidget::deleteTask);
    connect(selectDateTimeButton, &QPushButton::clicked, this, &TodoListWidget::showDateTimePicker);
    connect(taskList, &QListWidget::itemClicked, this, &TodoListWidget::onItemClicked);
    connect(taskList, &QListWidget::itemDoubleClicked, this, &TodoListWidget::editTask);
    connect(unmarkButton, &QPushButton::clicked, this, &TodoListWidget::unmarkTaskDone);
}

TodoListWidget::~TodoListWidget() {
    saveTasksToFile();
}


void TodoListWidget::saveTasksToFile() {
    const QColor doneBackgroundColor = QColor(80, 150, 50, 200);

    QFile file(filePath);

    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&file);
        for (int i = 0; i < taskList->count(); ++i) {
            QListWidgetItem *item = taskList->item(i);
            bool isCompleted = (item->background().color() == doneBackgroundColor);
            QString taskText = item->text();
            out << (isCompleted ? "1" : "0") << " " << taskText.trimmed() << "\n";
        }

        file.close();

    } else {

        qWarning() << "Не удалось открыть файл для записи:" << filePath << "Ошибка:" << file.errorString();
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


void TodoListWidget::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *inputLayout = new QHBoxLayout;

    taskInput->setPlaceholderText("Введите задачу...");
    dateTimeInput->setDateTime(QDateTime::currentDateTime());
    dateTimeInput->setDisplayFormat("dd.MM.yyyy HH:mm");

    dateTimeInput->setReadOnly(true);
    dateTimeInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
    selectDateTimeButton->setToolTip("Выбрать дату и время");


    QIcon dateTimeIcon(":/styles/calendar-clock.png");

    if (!dateTimeIcon.isNull()) {
        selectDateTimeButton->setIcon(dateTimeIcon);
        selectDateTimeButton->setText("");
        selectDateTimeButton->setFixedSize(selectDateTimeButton->sizeHint());
    }

    inputLayout->addWidget(taskInput);
    inputLayout->addWidget(selectDateTimeButton);
    inputLayout->addWidget(dateTimeInput);
    inputLayout->addWidget(addButton);

    mainLayout->addLayout(inputLayout);
    mainLayout->addWidget(taskList);
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(doneButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(unmarkButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

void TodoListWidget::onItemClicked(QListWidgetItem *item) {
    return;
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
    }
}

void TodoListWidget::showDateTimePicker() {
    DateTimePickerDialog dialog(dateTimeInput->dateTime(), this);
    if (dialog.exec() == QDialog::Accepted) {
        dateTimeInput->setDateTime(dialog.getSelectedDateTime());
    }
}


DateTimePickerDialog::DateTimePickerDialog(const QDateTime &initialDateTime, QWidget *parent)
    : QDialog(parent), currentDateTime(initialDateTime)
{
    setWindowTitle("Выберите дату и время");
    setModal(true);

    calendarWidget = new QCalendarWidget(this);
    calendarWidget->setSelectedDate(initialDateTime.date());
    calendarWidget->setGridVisible(true);
    calendarWidget->setStyleSheet(
        "QCalendarWidget QAbstractItemView:disabled { color: grey; }"
        ); //сделал серыми дни других недель, потому что если всё одно и то же = отвратительно
    calendarWidget->setHorizontalHeaderFormat(QCalendarWidget::NoHorizontalHeader);
    calendarWidget->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);


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

    updateDateTime();
}

void DateTimePickerDialog::updateDateTime() {
    currentDateTime.setDate(calendarWidget->selectedDate());
    currentDateTime.setTime(timeEdit->time());
}

QDateTime DateTimePickerDialog::getSelectedDateTime() const {
    return currentDateTime;
}
void TodoListWidget::unmarkTaskDone() {
     QList<QListWidgetItem*> items = taskList->selectedItems();
     if (items.isEmpty()) {
          QMessageBox::warning(this, "Ошибка", "Выберите задачу (или задачи) для снятия отметки!");
          return;
     }

     foreach(QListWidgetItem *item, items) {
        if (item->background().color() == QColor(80, 150, 50, 200)) {
            item->setForeground(palette().color(QPalette::Text));
            item->setBackground(QBrush());
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        }
    }
      saveTasksToFile();
}

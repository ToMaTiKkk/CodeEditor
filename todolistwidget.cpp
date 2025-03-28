#include "todolistwidget.h"
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QDateTimeEdit>
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

TodoListWidget::TodoListWidget(QWidget *parent)
    : QWidget(parent),
      taskList(new QListWidget(this)),
      taskInput(new QLineEdit(this)),
      dateTimeInput(new QDateTimeEdit(this)),
      addButton(new QPushButton("Добавить", this)),
      doneButton(new QPushButton("Выполнено", this)),
      deleteButton(new QPushButton("Удалить", this))
{
    setupUI();
    loadTasksFromFile();

    connect(addButton, &QPushButton::clicked, this, &TodoListWidget::addTask);
    connect(doneButton, &QPushButton::clicked, this, &TodoListWidget::markTaskDone);
    connect(deleteButton, &QPushButton::clicked, this, &TodoListWidget::deleteTask);
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
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            QStringList parts = line.split(" ", Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                bool isCompleted = parts[0] == "1";
                QString taskText = parts.mid(1).join(" ");

                QListWidgetItem *item = new QListWidgetItem(taskText, taskList);
                item->setFlags(item->flags() | Qt::ItemIsEditable);

                if (isCompleted) {
                    item->setForeground(QColor(255, 255, 255));
                    item->setBackground(QColor(80, 150, 50, 200)); //200 - прозрачность, насчет белой надо подумать
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                }
            }
        }
        file.close();
    } else {
        return;
    }
}


// Настройка UI
void TodoListWidget::setupUI() {
    /*setStyleSheet(R"(
        QWidget {
            background-color: #E3F2FD;
            font-family: Arial, sans-serif;
        }
        QPushButton {
            background-color: #64B5F6;
            color: white;
            border-radius: 8px;
            padding: 5px;
        }
        QPushButton:hover {
            background-color: #42A5F5;
        }
        QLineEdit {
            background-color: white;
            border: 1px solid #90CAF9;
            border-radius: 5px;
            padding: 5px;
        }
        QListWidget {
            background-color: white;
            border: 1px solid #90CAF9;
        }
        QListWidget::item {
            padding: 5px;
        }
        QListWidget::item:selected {
            background-color: #BBDEFB;
        }
        QDateTimeEdit {
             background-color: white;
             border: 1px solid #90CAF9;
             border-radius: 5px;
             padding: 4px;
        }
        QDateTimeEdit::up-button, QDateTimeEdit::down-button {
             width: 16px;
        }
    )");*/

    QVBoxLayout *mainLayout = new QVBoxLayout;
    QHBoxLayout *inputLayout = new QHBoxLayout;
    taskInput->setPlaceholderText("Введите задачу...");
    dateTimeInput->setCalendarPopup(true);
    dateTimeInput->setDateTime(QDateTime::currentDateTime());
    inputLayout->addWidget(taskInput);
    inputLayout->addWidget(dateTimeInput);
    inputLayout->addWidget(addButton);
    mainLayout->addLayout(inputLayout);
    mainLayout->addWidget(taskList);
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(doneButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    this->setLayout(mainLayout);

    /*taskList->setStyleSheet(R"(
        QListView::item {
            min-height: 32px;
            padding: 4px;
        }
        QListView QLineEdit {
            min-height: 32px;
            font-size: 14px;
            padding: 4px;
            border: 1px solid #007bff;
        }
    )");*/
}

void TodoListWidget::onItemClicked(QListWidgetItem *item) {
}

void TodoListWidget::addTask() {
    QString taskText = taskInput->text().trimmed();
    QDateTime deadline = dateTimeInput->dateTime();

    if (taskText.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите название задачи!");
        return;
    }

    QString taskItem = taskText + " | Дата: " + deadline.toString("dd.MM.yyyy HH:mm");
    QListWidgetItem *item = new QListWidgetItem(taskItem, taskList);
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
        foreach(QListWidgetItem *item, items) {
            delete taskList->takeItem(taskList->row(item));
        }
    }
}

void TodoListWidget::markTaskDone() {
     QList<QListWidgetItem*> items = taskList->selectedItems();
     if (items.isEmpty()) {
          QMessageBox::warning(this, "Ошибка", "Выберите задачу (или задачи) для отметки выполненной!");
          return;
     }

     foreach(QListWidgetItem *item, items) {
        // Проверяем, не выполнена ли уже
        if (item->foreground().color() != QColor(255, 255, 255)) {
            item->setForeground(QColor(255, 255, 255));
            item->setBackground(QColor(80, 150, 50, 200));
            item->setFlags(item->flags() & ~Qt::ItemIsEditable); //нельзя редачить
        }
    }
}

void TodoListWidget::editTask(QListWidgetItem *item) {
    if (item && (item->flags() & Qt::ItemIsEditable)) {
        taskList->editItem(item);
    }
}

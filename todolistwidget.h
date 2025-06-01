// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TODOLISTWIDGET_H
#define TODOLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QDateTimeEdit>
#include <QCalendarWidget>
#include <QDialog>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QListWidgetItem;
class QCloseEvent;
class QTimeEdit;
class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QDialogButtonBox;
QT_END_NAMESPACE

class DateTimePickerDialog;

class TodoListWidget : public QWidget {
    Q_OBJECT

public:
    TodoListWidget(QWidget *parent = nullptr);
    ~TodoListWidget();

private slots:
    void addTask();
    void deleteTask();
    void markTaskDone();
    void onItemClicked(QListWidgetItem *item);
    void showDateTimePicker();
    void editTask(QListWidgetItem *item);
    void unmarkTaskDone();

private:
    QListWidget *taskList;
    QLineEdit *taskInput;
    QDateTimeEdit *dateTimeInput;
    QPushButton *addButton;
    QPushButton *selectDateTimeButton;
    QPushButton *doneButton;
    QPushButton *deleteButton;
    QPushButton *unmarkButton;

    void setupUI();
    void saveTasksToFile();
    void loadTasksFromFile();

    const QString filePath = QDir::homePath() + "/tasks.txt";
};

class DateTimePickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit DateTimePickerDialog(const QDateTime &initialDateTime, QWidget *parent = nullptr);
    QDateTime getSelectedDateTime() const;

private slots:
    void updateDateTime();

private:
    QCalendarWidget *calendarWidget;
    QTimeEdit *timeEdit;
    QDialogButtonBox *buttonBox;
    QDateTime currentDateTime;
};


#endif

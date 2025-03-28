#ifndef TODOLISTWIDGET_H
#define TODOLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QDateTimeEdit>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QListWidgetItem;
class QCloseEvent;
QT_END_NAMESPACE

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
    void editTask(QListWidgetItem *item);

private:
    QListWidget *taskList;
    QLineEdit *taskInput;
    QDateTimeEdit *dateTimeInput;
    QPushButton *addButton;
    QPushButton *doneButton;
    QPushButton *deleteButton;
    void setupUI();
    void saveTasksToFile();
    void loadTasksFromFile();
    const QString filePath = QDir::homePath() + "/tasks.txt";
};

#endif // TODOLISTWIDGET_H

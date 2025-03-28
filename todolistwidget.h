#ifndef TODOLISTWIDGET_H
#define TODOLISTWIDGET_H

#include <QWidget> // Изменено с QMainWindow
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit> // Добавлено для taskInput
#include <QDateTimeEdit>
#include <QFile>      // Добавлено для QFile
#include <QTextStream>// Добавлено для QTextStream
#include <QDir>       // Добавлено для QDir

// Forward declaration для QListWidgetItem, если он используется только как указатель/ссылка в .h
QT_BEGIN_NAMESPACE
class QListWidgetItem;
class QCloseEvent; // Все еще нужно для деструктора, который вызывает save
QT_END_NAMESPACE

class TodoListWidget : public QWidget { // Изменено с QMainWindow
    Q_OBJECT

public:
    TodoListWidget(QWidget *parent = nullptr);
    ~TodoListWidget(); // Деструктор остался для сохранения

    // Можно сделать публичными, если нужно управлять снаружи
    // void saveTasksToFile();
    // void loadTasksFromFile();

private slots:
    void addTask();
    void deleteTask();
    void markTaskDone();
    // void saveTasks(); // Возможно, не нужен отдельный слот, если сохранение в деструкторе
    // void loadTasks(); // Возможно, не нужен отдельный слот, если загрузка в конструкторе
    void onItemClicked(QListWidgetItem *item);
    void editTask(QListWidgetItem *item); // Принимает QListWidgetItem*

private:
    QListWidget *taskList;
    QLineEdit *taskInput;
    QDateTimeEdit *dateTimeInput;
    QPushButton *addButton;
    QPushButton *doneButton;
    QPushButton *deleteButton;

    void setupUI();
    void saveTasksToFile();       // Сохранение задач в файл
    void loadTasksFromFile();     // Загрузка задач из файла

    // void closeEvent(QCloseEvent *event); // Удалено
    const QString filePath = QDir::homePath() + "/tasks.txt"; // Путь к файлу теперь здесь
};

#endif // TODOLISTWIDGET_H

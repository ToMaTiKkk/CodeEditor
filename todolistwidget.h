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

// Предварительные объявления для использования в классах
QT_BEGIN_NAMESPACE
class QListWidgetItem;
class QCloseEvent;
class QTimeEdit;
class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QDialogButtonBox;
QT_END_NAMESPACE

class DateTimePickerDialog; // Объявляем наш класс диалога

// --- Основной класс виджета ---
class TodoListWidget : public QWidget {
    Q_OBJECT

public:
    TodoListWidget(QWidget *parent = nullptr);
    ~TodoListWidget();

private slots:
    void addTask();
    void deleteTask();
    void markTaskDone();
    void onItemClicked(QListWidgetItem *item); // Слот пока пустой, но оставлен
    void showDateTimePicker(); // Слот для показа нашего диалога
    void editTask(QListWidgetItem *item);

private:
    QListWidget *taskList;
    QLineEdit *taskInput;
    QDateTimeEdit *dateTimeInput;
    QPushButton *addButton;
    QPushButton *selectDateTimeButton; // Новая кнопка для вызова диалога
    QPushButton *doneButton;
    QPushButton *deleteButton;

    void setupUI();
    void saveTasksToFile();
    void loadTasksFromFile();

    const QString filePath = QDir::homePath() + "/tasks.txt";
};


// --- Определение класса DateTimePickerDialog ---
// В идеале этот класс стоит вынести в отдельные файлы DateTimePickerDialog.h/.cpp
class DateTimePickerDialog : public QDialog {
    Q_OBJECT

public:
    // Конструктор принимает начальную дату/время
    explicit DateTimePickerDialog(const QDateTime &initialDateTime, QWidget *parent = nullptr);

    // Метод для получения выбранной даты/времени после закрытия диалога
    QDateTime getSelectedDateTime() const;

private slots:
    // Слот для обновления внутреннего значения QDateTime при изменении даты или времени
    void updateDateTime();

private:
    QCalendarWidget *calendarWidget;
    QTimeEdit *timeEdit;
    QDialogButtonBox *buttonBox; // Стандартные кнопки OK/Cancel
    QDateTime currentDateTime;   // Храним текущее выбранное значение даты и времени

    // Компоновка интерфейса диалога происходит в конструкторе
};


#endif // TODOLISTWIDGET_H

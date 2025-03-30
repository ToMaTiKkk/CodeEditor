#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QWidget> // Наш виджет будет на основе QWidget

// Прямые объявления для уменьшения зависимостей в заголовке
class QTermWidget; // Сам виджет терминала из библиотеки
class QVBoxLayout; // Layout для размещения QTermWidget внутри нашего виджета
class QKeyEvent;   // Для слота обработки клавиш
class QUrl;        // Для слота обработки ссылок

class TerminalWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget(); // Деструктор

    // Добавим метод для установки фокуса на поле ввода терминала (если нужно)
    void setInputFocus();

private slots:
    // Слоты для обработки сигналов от вложенного QTermWidget
    void handleKeyPress(const QKeyEvent *event);
    void handleLinkActivation(const QUrl &url, bool fromContextMenu);

private:
    QTermWidget *term_widget; // Указатель на реальный виджет терминала
    QVBoxLayout *layout;      // Layout для размещения term_widget
};

#endif // TERMINALWIDGET_H

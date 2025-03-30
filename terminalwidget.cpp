#include "terminalwidget.h"
#include <qtermwidget.h>
#include <QVBoxLayout>
#include <QFont>
#include <QKeySequence>
#include <QKeyEvent>
#include <QUrl>
#include <QDesktopServices>
#include <QApplication> // Для QApplication::font()
#include <QDebug>       // Для отладки

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent) // Инициализируем базовый класс
{
    // 1. Создаем сам QTermWidget
    // Указываем 'this' как родителя, чтобы он удалился вместе с нашим TerminalWidget
    term_widget = new QTermWidget(this);
    if (!term_widget) {
        qCritical() << "Failed to create internal QTermWidget!";
        return; // Выходим, если не удалось создать
    }
    term_widget->setObjectName("internalTermWidget"); // Имя для стилей/отладки

    // 2. Настраиваем QTermWidget (код из твоего тестового main())
    QFont termFont = QApplication::font(); // Берем шрифт приложения как базу
    termFont.setFamily("Monospace");       // Установите РЕАЛЬНЫЙ моноширинный шрифт!
    termFont.setPointSize(12);             // Ваш размер
    term_widget->setTerminalFont(termFont);

    term_widget->setColorScheme("Tango"); // Ваша цветовая схема
    term_widget->setBlinkingCursor(true);
    term_widget->setMargin(5);             // Ваши отступы

    // 3. Создаем Layout для нашего TerminalWidget
    // Указываем 'this' как родителя, он будет управлять layout'ом
    layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0); // Убираем отступы у самого layout'а
    layout->addWidget(term_widget);         // Добавляем QTermWidget в layout

    // Подключение сигналы от вложенного QTermWidget к приватным слотам
    connect(term_widget, &QTermWidget::termKeyPressed, this, &TerminalWidget::handleKeyPress);
    connect(term_widget, &QTermWidget::urlActivated, this, &TerminalWidget::handleLinkActivation);

    qDebug() << "TerminalWidget wrapper created and configured.";
}

TerminalWidget::~TerminalWidget()
{
    qDebug() << "TerminalWidget wrapper destroyed.";
}

// Публичный метод для установки фокуса
void TerminalWidget::setInputFocus()
{
    if (term_widget) {
        term_widget->setFocus();
    }
}

// Приватные слоты для обработки сигналов

void TerminalWidget::handleKeyPress(QKeyEvent *event)
{
    if (!term_widget) return;

    if (event->matches(QKeySequence::Copy)) {
        term_widget->copyClipboard();
        event->accept();
    } else {
        event->ignore();
    }
}

void TerminalWidget::handleLinkActivation(const QUrl &url, bool fromContextMenu)
{
    Q_UNUSED(fromContextMenu); // Пока не используем этот параметр
    if (url.isValid()) {
        qDebug() << "Opening link:" << url.toString();
        QDesktopServices::openUrl(url); // Открываем ссылку в браузере
    }
}

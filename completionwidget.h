#ifndef COMPLETIONWIDGET_H
#define COMPLETIONWIDGET_H

#include <QListWidget>
#include "lspmanager.h" // нужнео для LspCompletionItem
#include <QPlainTextEdit>

class CompletionWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit CompletionWidget(QPlainTextEdit* editor, QWidget *parent);

    void updateItems(const QList<LspCompletionItem>& items); // заполнить список
    void triggerSelection(); //выбрать текущий элемент (по Enter/Tab)
    void triggerSelectionFromItem(QListWidgetItem *item); // ывбрать конкретный

    void filterItems(const QString& prefix); // метод для фильтрации

    // методы внешнего управления
    void navigateUp();
    void navigateDown();
    void navigatePageUp();
    void navigatePageDown();

signals:
    void completionSelected(const QString& textToInsert); // сигнал о выборе

protected:
    void focusOutEvent(QFocusEvent *event) override; // скрывать при потере фокуса
    void hideEvent(QHideEvent *event) override;

private slots:
    void onItemDoubleClicked(QListWidgetItem *item); // выбор по дабл-клику

private:
    // храним оригинальный список для фильтрации
    QList<LspCompletionItem> m_originalItems;
    QMap<QListWidgetItem*, LspCompletionItem> m_itemData; // хранение текста для вставки
    QPlainTextEdit* m_editor; // указатель на сам редактор

    int findNextVisibleRow(int startRow, int step); // найти следующий или предыдущий видимый
};

#endif

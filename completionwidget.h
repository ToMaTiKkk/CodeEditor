#ifndef COMPLETIONWIDGET_H
#define COMPLETIONWIDGET_H

#include <QListWidget>
#include "lspmanager.h" // нужнео для LspCompletionItem

class CompletionWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit CompletionWidget(QWidget *parent = nullptr);
    void updateItems(const QList<LspCompletionItem>& items); // заполнить список
    void triggerSelection(); //выбрать текущий элемент (по Enter/Tab)
    void triggerSelectionFromItem(QListWidgetItem *item);

    void filterItems(const QString& prefix); // метод для фильтрации

    void keyPressEvent(QKeyEvent *event) override; // перехват Enter/Tab/Esc

signals:
    void completionSelected(const QString& textToInsert); // сигнал о выборе

protected:
    void focusOutEvent(QFocusEvent *event) override; // скрывать при потере фокуса

private slots:
    void onItemDoubleClicked(QListWidgetItem *item); // выбор по дабл-клику

private:
    // храним оригинальный список для фильтрации
    QList<LspCompletionItem> m_originalItems;
    QMap<QListWidgetItem*, LspCompletionItem> m_itemData; // хранение текста для вставки
};

#endif

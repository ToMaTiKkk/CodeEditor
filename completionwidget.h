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
    void handleKeyEvent(QKeyEvent *event); // обработка навигации
    void triggerSelection(); // выбрать текущий элемент (по Enter/Tab)

signals:
    void completionSelected(const QString& textToInsert); // сигнал о выборе

protected:
    void keyPressEvent(QKeyEvent *event) override; // перехват Enter/Tab/Esc
    void focusOutEvent(QFocusEvent *event) override; // скрывать при потере фокуса

private slots:
    void onItemDoubleClicked(QListWidgetItem *item); // выбор по дабл-клику

private:
    QMap<QListWidgetItem*, QString> m_itemInsertText; // хранение текста для вставки
};

#endif

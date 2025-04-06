#include "completionwidget.h"
#include <QKeyEvent>
#include <QDebug>

CompletionWidget::CompletionWidget(QWidget *parent) : QListWidget(parent)
{
    setWindowFlags(Qt::ToolTip); // делаем похожим на всплывашку
    setFocusPolicy(Qt::StrongFocus); // чтобы мог получать фокус
    connect(this, &QListWidget::itemDoubleClicked, this, &CompletionWidget::onItemDoubleClicked);
}

void CompletionWidget::updateItems(const QList<LspCompletionItem>& items)
{
    clear(); // очищаем старый список
    m_itemInsertText.clear();
    for (const auto& item : items) {
        // TODO: добавить иконки в зависимости от item.kind
        QListWidgetItem *listItem = new QListWidgetItem(item.label);
        listItem->setToolTip(item.detail + "\n---\n" + item.documentation); // показывае доп инфо в тултипу
        m_itemInsertText[listItem] = item.insertText; // сохраняем текст для вставки
        addItem(listItem);
    }
    if (count() > 0) {
        setCurrentRow(0); // выделяем первый элемент
    }
}

void CompletionWidget::handleKeyEvent(QKeyEvent *event)
{
    QListWidget::keyPressEvent(event);
}

void CompletionWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Tab) {
        triggerSelection();
    } else if (event->key() == Qt::Key_Escape) {
        hide();
    } else {
        // передаем обработку навигации (стрелки) родительскому классу
        QListWidget::keyPressEvent(event);
    }
}

void CompletionWidget::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event);
    hide(); // скрываем, если кликнули мимо
}

void CompletionWidget::triggerSelection()
{
    if (currentItem()) {
        emit completionSelected(m_itemInsertText.value(currentItem()));
        hide();
    }
}

void CompletionWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    if (item) {
        emit completionSelected(m_itemInsertText.value(item));
        hide();
    }
}

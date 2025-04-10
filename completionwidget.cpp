#include "completionwidget.h"
#include <QKeyEvent>
#include <QDebug>
#include <QApplication> // для QApplication::sendEvent

CompletionWidget::CompletionWidget(QPlainTextEdit* editor, QWidget *parent) : QListWidget(parent), m_editor(editor)
{
    // setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint); // делаем похожим на всплывашку, стиль без рамки
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setSelectionMode(QAbstractItemView::SingleSelection); // только один элемент может быть выбран
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // горизонтальный скорлл не нужен
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded); // вертикальный по необходимости

    connect(this, &QListWidget::itemDoubleClicked, this, &CompletionWidget::onItemDoubleClicked);
    // сигнал улобне для Enter
    connect(this, &QListWidget::itemActivated, this, &CompletionWidget::triggerSelectionFromItem);
}

// храним LspCompletionItemd целиком, чтобы иметь доступ ко всем данным при выборе/филтрации
void CompletionWidget::updateItems(const QList<LspCompletionItem>& items)
{
    const QSignalBlocker blocker(this); // блок сигналов на время обновления
    clear(); // очищаем старый список
    m_itemData.clear();
    m_originalItems = items; // сохраняем оригинальный список

    for (const auto& item : m_originalItems) {
        // TODO: добавить иконки в зависимости от item.kind
        QListWidgetItem *listItem = new QListWidgetItem(item.label);
        QString tooltip = item.detail;
        if (!item.documentation.isEmpty()) {
            tooltip += "\n---\n" + item.documentation;
        }
        listItem->setToolTip(tooltip); // показывае доп инфо в тултипe

        // сохраняем ВСЕ данные элемента
        m_itemData[listItem] = item;
        addItem(listItem);

        // убедимя что все элементы изначально видимы
        listItem->setHidden(false);
    }

    if (count() > 0) {
        // устанвливаем текущий элемент на первый после обновы или фильтрации и прокручивем к нему
        setCurrentRow(0, QItemSelectionModel::SelectCurrent);
        scrollToItem(currentItem(), QAbstractItemView::PositionAtTop);
    }
    // обновляем геометрию
    adjustSize(); // может помочь с авто-размером
}

// фильтрация
void CompletionWidget::filterItems(const QString& prefix)
{
    qDebug() << "Филтрация по префиксу:" << prefix;
    // TODO: реальную фильтрацию сделать
    // 1. скрывать все элементы или создаватьь новый список
    // 2. проходится по оригиналу
    // 3. если поле начинается с префикса (сделать регистрозависимым может), то сделать соответвутсвующее поле видимым или добавить его в новый список
    // 4. если фильтрация пустая - то скрывает
    // 5. а иначе выбираем первый видимый элемент

    // пока что просто очищаем и заполняем заново всем списком, без фильтрации
    updateItems(m_originalItems);
    if (count() > 0) {
        setCurrentRow(0);
    } else {
        hide(); // скрыть если пустой
    }
}

void CompletionWidget::focusOutEvent(QFocusEvent *event)
{
    hide(); // скрываем, если кликнули мимо
    QListWidget::focusOutEvent(event);
}

int CompletionWidget::findNextVisibleRow(int startRow, int step)
{
    int countLimit = count();
    if (countLimit == 0) return -1;

    int currentRow = startRow;
    // начинаем проверка со следующей или предыдущей строки
    currentRow += step;

    // прокручиваем весь списко пока не найден видимыф блок или пока не пройдем весь список
    for (int i = 0; i < countLimit; ++i) {
        // проверка выхода за границы списка и зацикливание
        if (currentRow < 0) {
            currentRow = countLimit - 1;
        } else if (currentRow >= countLimit) {
            currentRow = 0;
        }

        // проверяем видимость элемента на текущем индексе
        QListWidgetItem* currentItemWidget = item(currentRow);
        if (currentItemWidget && !currentItemWidget->isHidden()) {
            return currentRow; // нашли видимый виджет
        }

        currentRow += step;
    }

    // если прошли весь списко и не нашли другого видимого виджета, то вернем исходный или -1
    QListWidgetItem* startItemWidget = item(startRow);
    if (startRow >= 0 && startItemWidget && !startItemWidget->isHidden()) {
        return startRow;
    } else {
        return -1; // видимых элементов нет или только стартовый
    }
}

void CompletionWidget::navigateUp()
{
    if (count() == 0) return;

    int current = currentRow();
    int next = findNextVisibleRow(current, -1); // ищем вверхе поэтому -1
    if (next != -1) {
        setCurrentRow(next);
        scrollToItem(item(next), QAbstractItemView::EnsureVisible);
    }
}

void CompletionWidget::navigateDown()
{
    if (count() == 0) return;

    qDebug() << "      [COMPLETION WIDGET] navigateDown() called.";
    int current = currentRow();
    int next = findNextVisibleRow(current, 1); // ищем вверхе поэтому -1
    if (next != -1) {
        setCurrentRow(next);
        scrollToItem(item(next), QAbstractItemView::EnsureVisible);
        qDebug() << "      [COMPLETION WIDGET] navigateDown() - Row set to:" << next;
    }
}

void CompletionWidget::navigatePageUp()
{
    if (count() == 0) return;

    int current = currentRow();
    // найти видимый элемент на страницу выше
    int itemsPerPage = qMax(1, height() / sizeHintForRow(0)); // приблизительно
    int targetRow = qMax(0, current - itemsPerPage);
    int step = -1;
    int next = current;
    // ищем ближайщий видимый сверху или равный targetRow
    while (true) {
        int tempNext = findNextVisibleRow(next, step);
        if (tempNext == -1 || tempNext == next || tempNext > current) break; // не нашли или зациклились
        next = tempNext;
        if (next <= targetRow) break; // нашли достаточно далеко вверху
    }

    // если не нашли далеко, но хоть что то нашли сверху, то берем
    if (next == current) { // если ничего не изменилось то берем самый верхний
        next = findNextVisibleRow(0, 1); // первый видимый
    } 

    if (next != -1 && next != current) {
        setCurrentRow(next);
        scrollToItem(item(next), QAbstractItemView::PositionAtTop);
    } else if (next == -1 && current != 0) { // если видимых выше нет, но мы не на самом верху, то идем наверх
        next = findNextVisibleRow(0, 1);
        if (next != -1) setCurrentRow(next);

    }
}

void CompletionWidget::navigatePageDown()
{
    if (count() == 0) return;

    int current = currentRow();
    // найти видимый элемент на страницу ниже
    int itemsPerPage = qMax(1, height() / sizeHintForRow(0)); // приблизительно
    int targetRow = qMax(count() - 1, current + itemsPerPage);
    int step = 1;
    int next = current;
    // ищем ближайщий видимый снизу или равный targetRow
    while (true) {
        int tempNext = findNextVisibleRow(next, step);
        if (tempNext == -1 || tempNext == next || tempNext < current) break; // не нашли или зациклились
        next = tempNext;
        if (next >= targetRow) break; // нашли достаточно далеко внизу
    }

    // если не нашли далеко, но хоть что то нашли снизу, то берем
    if (next == current) { // если ничего не изменилось то берем самый нижний
        next = findNextVisibleRow(count() - 1, -1); // последний видимый
    } 

    if (next != -1 && next != current) {
        setCurrentRow(next);
        scrollToItem(item(next), QAbstractItemView::PositionAtBottom);
    } else if (next == -1 && current != count() - 1) { // если видимых выше нет, но мы не на самом низу, то идем вниз
        next = findNextVisibleRow(count() - 1, -1);
        if (next != -1) setCurrentRow(next);

    }
}

// вызывается при Ентер и Таб
void CompletionWidget::triggerSelection()
{
    qDebug() << "      [COMPLETION WIDGET] triggerSelection() called.";
    if (currentItem() && !currentItem()->isHidden()) {
        triggerSelectionFromItem(currentItem());
    } else if (count() > 0) {
        int firstVisible = findNextVisibleRow(-1, 1); // найти самый первый (-1 начало, и по одному внизу)
        if (firstVisible != -1) {
            setCurrentRow(firstVisible);
            triggerSelectionFromItem(currentItem());
        } else {
            hide(); // нет видимых для выбора
        }
    } else {
        hide(); // списка нет
    }
}

// слот для сигнала itemActivated (Enter) или по doubleClicked
void CompletionWidget::triggerSelectionFromItem(QListWidgetItem *item)
{
    qDebug() << "      [COMPLETION WIDGET] triggerSelectionFromItem() called for item:" << (item ? item->text() : "NULL");
    if (item && m_itemData.contains(item)) {
        QString text = m_itemData.value(item).insertText.isEmpty() ? item->text() : m_itemData.value(item).insertText;
        qDebug() << "      [COMPLETION WIDGET] Emitting completionSelected with text:" << text;
        emit completionSelected(text); // отправляем текст вставки
        hide();
        qDebug() << "      [COMPLETION WIDGET] Hidden after selection.";
    } else {
        qDebug() << "      [COMPLETION WIDGET] Item invalid or not found in map.";
        hide(); 
    }
}

void CompletionWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    // двойной клик все тоже самое что и ентер
    triggerSelectionFromItem(item);
}

void CompletionWidget::hideEvent(QHideEvent *event)
{
    // оставляем стандартную обработку
    QListWidget::hideEvent(event);
}
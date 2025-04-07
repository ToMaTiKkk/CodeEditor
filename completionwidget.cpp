#include "completionwidget.h"
#include <QKeyEvent>
#include <QDebug>
#include <QApplication> // для QApplication::sendEvent

CompletionWidget::CompletionWidget(QWidget *parent) : QListWidget(parent)
{
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint); // делаем похожим на всплывашку, стиль без рамки
    setFocusPolicy(Qt::StrongFocus); // чтобы мог получать фокус, важно для приема событий клавы
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

// обработчик нажатий
void CompletionWidget::keyPressEvent(QKeyEvent *event)
{
    if (count() == 0) { // если виджет пустой, то ничего не делаем
        if (event->key() == Qt::Key_Escape) {
            hide();
            event->accept();
        } else {
            event->ignore();
        }
        return;
    }

    switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
            // проверяем выбранный видимый элемент
            if (currentItem() && !currentItem()->isHidden()) {
                triggerSelection(); // вызвать выбор текущего элемента
            } else {
                // если ничего не выбрано или выбран скрытй, то пытаемся выбрать первый видимый (если он есть)
                bool selected = false;
                for (int i = 0; i < count(); ++i) {
                    if (!item(i)->isHidden()) {
                        setCurrentRow(i); // выбираем первывй видимый
                        triggerSelection();
                        selected = true;
                        break;
                    }
                }
                if (!selected) {
                    hide(); // если элемента не оказалось, то просто скрываем
                }
            }
            event->accept(); // событие обработано
            break;

        case Qt::Key_Escape:
            hide();
            event->accept();
            break;

        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            // передаем навигацию стандартному обработчику списка
            QListWidget::keyPressEvent(event);
            // Явно принимаем событие, чтобы оно не ушло дальше редактору
            event->accept();
            break;

        default:
            // все остальные клавиши с буквами, цифрами и символам, это не навигация, клиентская часть должна решить, что делать (фильтровать, скрыть, передеать и тп)
            event->ignore(); // событие дальше пойдет в eventFilter in MainWindow
            break;
    }
}

void CompletionWidget::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event);
    hide(); // скрываем, если кликнули мимо
    QListWidget::focusOutEvent(event);
}

// вызывается при Ентер и Таб
void CompletionWidget::triggerSelection()
{
    if (currentItem()) {
        triggerSelectionFromItem(currentItem());
    } else if (count() > 0) {
        // если текущего нет, но элементы есть, то выберем первый
        setCurrentRow(0);
        triggerSelectionFromItem(currentItem());
    } else {
        hide(); // списка нет
    }
}

// слот для сигнала itemActivated (Enter) или по doubleClicked
void CompletionWidget::triggerSelectionFromItem(QListWidgetItem *item)
{
    if (item && m_itemData.contains(item)) {
        emit completionSelected(m_itemData.value(item).insertText); // отправляем текст вставки
        hide();
    }
}

void CompletionWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    // двойной клик все тоже самое что и ентер
    triggerSelectionFromItem(item);
}

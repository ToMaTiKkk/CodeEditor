#include "completionwidget.h"
#include <QKeyEvent>
#include <QDebug>
#include <QApplication> // для QApplication::sendEvent
#include <QTextBlock>
#include <QTimer>
#include <algorithm>
#include <functional>

// реализация PrefixFilterStrategy
int PrefixFilterStrategy::match(const QString& query, const LspCompletionItem& item) const {
    if (query.isEmpty()) {
        return 100; // если запрос пустой, все элементы подходят
    }

    QString label = item.label.toLower();
    QString q = query.toLower();

    // прямое совпадение с началом строки
    if (label.startsWith(q)) {
        if (label.length() == q.length()) {
            return 100;
        }
        // чем ближе длина запроса к длине элемента, тем выше балл
        return 80 + 20 * q.length() / label.length();
    }

    // если не с начала строки, но запрос содержит
    if (label.contains(q)) {
        // чем ближе к началу тем выше балл
        int pos = label.indexOf(q);
        int score = 50 - (pos * 40 / qMax(1, label.length()));
        return qMax(10, score); // минимум 10 баллов если содержит
    }

    // поиск по словам (CamelCase/snake_case)
    QStringList words;
    QString word;
    for (int i = 0; i < label.length(); ++i) {
        if (label[i].isUpper() || label[i] == '_') {
            if (!word.isEmpty()) {
                words.append(word);
                word.clear();
            }
            if (label[i] != '_') {
                word += label[i];
            }
        } else {
            word += label[i];
        }
    }
    if (!word.isEmpty()) {
        words.append(word);
    }

    // проверяем начинается ли какое либо слово с запроса
    for (const QString& w : words) {
        if (w.startsWith(q)) {
            return 60; // неплохое совпаени, но не с начала
        }
    }

    return 0; // не подходит
}

// реализация FuzzyFilterStrategy
int FuzzyFilterStrategy::match(const QString& query, const LspCompletionItem& item) const {
    if (query.isEmpty()) {
        return 100;
    }

    QString label = item.label;
    int baseScore = calculateFuzzyScore(query.toLower(), label.toLower());

    // проверяем документации, но с меньшим весом
    if (!item.documentation.isEmpty()) {
        int docScore = calculateFuzzyScore(query.toLower(), item.documentation.toLower()) / 2;
        baseScore = qMax(baseScore, docScore);
    }

    // проверяем детали с ещё меньшим весом
    if (!item.detail.isEmpty()) {
        int detailScore = calculateFuzzyScore(query.toLower(), item.detail.toLower()) / 3;
        baseScore = qMax(baseScore, detailScore);
    }

    return baseScore;
}

int FuzzyFilterStrategy::calculateFuzzyScore(const QString& query, const QString& target) const {
    if (query.isEmpty()) {
        return 100;
    }
    if (target.isEmpty()) {
        return 0;
    }
    // точно совпадение
    if (target == query) {
        return 100;
    }
    // если содержит как подстановку
    if (target.contains(query)) {
        // в начале строки - выше балл
        if (target.startsWith(query)) {
            return 90 + 10 * query.length() / target.length();
        }

        // в середине - средний балл
        return 70 + 10 * query.length() / target.length();
    }

    // АЛГОРИТМ нечеткого соответствия
    // находим все символа запроса в порядке их следование в target
    int targetIndex = 0;
    int matched = 0;

    for (int i = 0; i < query.length(); ++i) {
        bool found = false;
        for (int j = targetIndex; j < target.length(); ++j) {
            if (query[i].toLower() == target[j].toLower()) {
                // нашли совпадение
                targetIndex = j + 1;
                matched++;
                found = true;
                break;
            }
        }
        if (!found) break;
    }

    // все символы найдены в правильном порядке
    if (matched == query.length()) {
        // рассчитываем балл в зависимости от плотности совпадения
        float density = static_cast<float>(query.length()) / targetIndex;
        return 50 + static_cast<int>(density * 40);
    }

    // частичное совпадение
    float partialMatch = static_cast<float>(matched) / query.length();
    return static_cast<int>(partialMatch * 40);
}

// реализация ContextFIlterStrategy
int ContextFilterStrategy::match(const QString& query, const LspCompletionItem& item) const {
    // базовое совпадение с использованием нечеткого поиска
    FuzzyFilterStrategy fuzzy;
    int baseScore = fuzzy.match(query, item);

    // учитываем испторию использования
    int usageCount = m_usageCount.value(item.label, 0);
    int usageBonus = qMin(20, usageCount * 5); // до 20% бонус за частое использование

    // учитываем тип элемента (kind) (функция, переменная и др)
    int kindBonus = 0;
    switch (item.kind) {
        case 3: // функция
            kindBonus = 10; 
            break;
        case 6: // переменная
            kindBonus = 5;
            break;
        default:
            kindBonus = 0;
    }

    return qMin(100, baseScore + usageBonus + kindBonus);
}

void ContextFilterStrategy::addToHistory(const LspCompletionItem& item) {
    m_usageCount[item.label] = m_usageCount.value(item.label, 0) + 1;
}

CompletionWidget::CompletionWidget(QPlainTextEdit* editor, QWidget *parent) 
    : QListWidget(parent)
    , m_editor(editor)
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

    // инициализация стратегий фильтрации
    m_filterStrategies.push_back(std::make_shared<PrefixFilterStrategy>());
    m_filterStrategies.push_back(std::make_shared<FuzzyFilterStrategy>());
    m_filterStrategies.push_back(std::make_shared<ContextFilterStrategy>());

    // по умолчанию используем нечеткий поиск
    m_currentStrategy = m_filterStrategies[1];

    // настрйока кжша (макс 50)
    m_filterCache.setMaxCost(50);
}

CompletionWidget::~CompletionWidget() {
    m_filterCache.clear();
}

// храним LspCompletionItemd целиком, чтобы иметь доступ ко всем данным при выборе/филтрации
void CompletionWidget::updateItems(const QList<LspCompletionItem>& items)
{
    const QSignalBlocker blocker(this); // блок сигналов на время обновления
    clear(); // очищаем старый список
    m_itemData.clear();
    m_originalItems = items; // сохраняем оригинальный список
    m_filterCache.clear();

    // обновляем контекст для улучшения фильтрации
    updateContext();

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
    
    if (m_originalItems.isEmpty()) {
        hide();
        return;
    }

    // проверяем кэш для этого запроса
    if (m_filterCache.contains(prefix)) {
        const QList<FilterResult>* cachedResults = m_filterCache.object(prefix);

        // применяем кэшированные резы
        const QSignalBlocker blocker(this); // блокируем сигналы на время обновы
        clear();
        m_itemData.clear();

        for (const FilterResult& result : *cachedResults) {
            if (result.score >= m_filterThreshold) {
                QListWidgetItem *listItem = new QListWidgetItem(result.lspItem.label);

                // добавление тултупи
                QString tooltip = result.lspItem.detail;
                if (!result.lspItem.documentation.isEmpty()) {
                    tooltip += "\n---\n" + result.lspItem.documentation;
                }
                listItem->setToolTip(tooltip);

                // заполянем данные
                m_itemData[listItem] = result.lspItem;
                addItem(listItem);
            }
        }
    } else {
        // выполняем фильтрацию
        QList<FilterResult> results = performFiltering(prefix);
        // кэшируем резы
        m_filterCache.insert(prefix, new QList<FilterResult>(results), results.size());

        // применяем резы
        const QSignalBlocker blocker(this); // блокируем сигналы на время обновы
        clear();
        m_itemData.clear();

        for (const FilterResult& result : results) {
            if (result.score >= m_filterThreshold) {
                QListWidgetItem *listItem = new QListWidgetItem(result.lspItem.label);

                // добавление тултупи
                QString tooltip = result.lspItem.detail;
                if (!result.lspItem.documentation.isEmpty()) {
                    tooltip += "\n---\n" + result.lspItem.documentation;
                }
                // для отладки показываем балл фильтрации
                tooltip += QString("\nРелевантность: %1%").arg(result.score);
                listItem->setToolTip(tooltip);

                // заполянем данные
                m_itemData[listItem] = result.lspItem;
                addItem(listItem);
            }
        }
    }

    // показывем или скрываем виджет в зависимости от реза
    if (count() > 0) {
        setCurrentRow(0);
        scrollToItem(currentItem(), QAbstractItemView::PositionAtTop);
        if (!isVisible()) {
            show();
        }
    } else {
        hide(); // если пустой
    }

    // обновили геометрию
    adjustSize();
}

void CompletionWidget::focusOutEvent(QFocusEvent *event)
{
    hide(); // скрываем, если кликнули мимо
    QListWidget::focusOutEvent(event);
}

QList<FilterResult> CompletionWidget::performFiltering(const QString& query) {
    QList<FilterResult> results;

    // если запрос пустой, то показываем все элементы с макс баллом
    if (query.isEmpty()) {
        for (const auto& item : m_originalItems) {
            FilterResult result;
            result.lspItem = item;
            result.item = nullptr; // не создаем элемент виджета на этом этапе
            result.score = 100;
            results.append(result);
        }
        return results;
    }

    // фильтруем с использванием выбранной стратегии
    for (const auto& item : m_originalItems) {
        int score = m_currentStrategy->match(query, item);

        if (score >= m_filterThreshold) {
            FilterResult result;
            result.lspItem = item;
            result.item = nullptr;
            result.score = score;
            results.append(result);
        }
    }

    // сортируем результуты по убывани баллов
    std::sort(results.begin(), results.end());

    return results;
}

void CompletionWidget::updateContext() {
    if (!m_editor) return;

    QTextCursor cursor = m_editor->textCursor();
    QTextBlock block = cursor.block();

    // получаем текст текущей строки до курсора
    QString lineText = block.text();
    int columnIndex = cursor.positionInBlock();
    m_currentContext = lineText.left(columnIndex);

    qDebug() << "Обновлен контекст:" << m_currentContext;
}

void CompletionWidget::setCurrentFilterStrategy(const QString& strategyName) {
    for (const auto& strategy : m_filterStrategies) {
        if (strategy->name() == strategyName) {
            m_currentStrategy = strategy;
            // кэш при смене стратгеии очищаем
            m_filterCache.clear();
            return;
        }
    }

    // если стратегия не найдена, то используем нечеткий поиск по умолчанию
    qDebug() << "Стратегия не найдена:" << strategyName << ".Используется нечеткий поиск по умолчанию";
    m_currentStrategy = m_filterStrategies[1];
}

QStringList CompletionWidget::availableFilterStrategies() const {
    QStringList result;
    for (const auto& strategy : m_filterStrategies) {
        result.append(strategy->name());
    }
    return result;
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
        
        // добавляем в историю для контекстной стратегии
        for (const auto& strategy : m_filterStrategies) {
            auto contextStrategy = std::dynamic_pointer_cast<ContextFilterStrategy>(strategy);
            if (contextStrategy) {
                contextStrategy->addToHistory(m_itemData.value(item));
            }
        }

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

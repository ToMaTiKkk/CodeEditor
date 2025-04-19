#include "completionwidget.h"
#include <QKeyEvent>
#include <QDebug>
#include <QApplication> // для QApplication::sendEvent
#include <QTextBlock>
#include <QTimer>
#include <algorithm> // std::sort
#include <functional>
#include <QtGlobal> // qCompareCaseInsensitive

// реализация PrefixFilterStrategy
int PrefixFilterStrategy::match(const QString& query, const LspCompletionItem& item, const CompletionScoringConfig& config) const {
    if (query.isEmpty()) {
        return config.prefixExactMatchScore; // если запрос пустой, все элементы подходят
    }

    QString label = item.label.toLower();
    QString q = query.toLower();

    // прямое совпадение с началом строки
    if (label.startsWith(q)) {
        if (label.length() == q.length()) {
            return config.prefixExactMatchScore; // 100
        }
        // начинается с запроса но длинне
        int score = config.prefixStartMatchBase + config.prefixStartLengthBonusScale * q.length() / label.length();
        // ограничивает сверху точным совпадением
        return qMin(config.prefixExactMatchScore, score);
    }

    // если не с начала строки, но запрос содержит
    if (label.contains(q)) {
        // чем ближе к началу тем выше балл
        int pos = label.indexOf(q);
        // базовый блл минус штраф за позицию
        int score = config.prefixContainsBase - config.prefixContainsPosPenaltyScale * pos / qMax(1, label.length());
        return qMax(config.prefixMinScoreIfContains, score); // минимум 10 баллов если содержит
    }

    // поиск по словам (CamelCase/snake_case)
    QStringList words;
    QString currentWord;
    for (int i = 0; i < label.length(); ++i) { // используем ориг лейбл для CamelCase
        QChar c = item.label[i];
        if (c.isUpper() && i > 0) { // нашли заглавную но не первую
            if (!currentWord.isEmpty()) {
                words.append(currentWord.toLower());
            }
            currentWord = c;
        } else if (c == '_') { // нашли разделитель
            if (!currentWord.isEmpty()) {
                words.append(currentWord.toLower());
            }
            currentWord.clear();
        } else {
            currentWord += c;
        }
    }
    if (!currentWord.isEmpty()) {
        words.append(currentWord.toLower());
    }

    // проверяем начинается ли какое либо слово с запроса
    for (const QString& w : words) {
        if (w.startsWith(q)) {
            return config.prefixWordStartMatchScore; // неплохое совпаени, но не с начала
        }
    }

    return 0; // не подходит ни по одному критерию
}

// реализация FuzzyFilterStrategy
int FuzzyFilterStrategy::match(const QString& query, const LspCompletionItem& item, const CompletionScoringConfig& config) const {
    if (query.isEmpty()) {
        return config.fuzzyExactMatchScore; //100
    }

    int baseScore = calculateFuzzyScore(query.toLower(), item.label.toLower(), config);

    // проверяем документации, применяем множитель
    if (!item.documentation.isEmpty() && config.fuzzyDocMatchMultiplier > 0) {
        int docScore = calculateFuzzyScore(query.toLower(), item.documentation.toLower(), config);
        baseScore = qMax(baseScore, static_cast<int>(docScore * config.fuzzyDocMatchMultiplier));
    }

    // проверяем детали, применяем множитель
    if (!item.detail.isEmpty() && config.fuzzyDetailMatchMultiplier > 0) {
        int detailScore = calculateFuzzyScore(query.toLower(), item.detail.toLower(), config);
        baseScore = qMax(baseScore, static_cast<int>(detailScore * config.fuzzyDetailMatchMultiplier));
    }

    return baseScore;
}

int FuzzyFilterStrategy::calculateFuzzyScore(const QString& query, const QString& target, const CompletionScoringConfig& config) const {
    if (query.isEmpty()) {
        return config.fuzzyExactMatchScore; //100
    }
    if (target.isEmpty()) {
        return 0;
    }
    // точно совпадение
    if (target == query) {
        return config.fuzzyExactMatchScore; //100
    }
    // содержит как префикс
    if (target.startsWith(query)) {
        int score = config.fuzzyPrefixMatchBase + config.fuzzyPrefixLengthBonusScale * query.length() / target.length();
        return qMin(config.fuzzyExactMatchScore, score); // ограничиваем 100
    }
    // если содержит как подстроку (не в начале)
    if (target.contains(query)) {
        int score = config.fuzzyPrefixMatchBase + config.fuzzyPrefixLengthBonusScale * query.length() / target.length();
        // ограничиваем баллом чуть ниже префиксного совпадения
        return qMin(config.fuzzyPrefixMatchBase > 0 ? config.fuzzyPrefixMatchBase - 1 : 0, score);
    }

    // АЛГОРИТМ нечеткого соответствия
    // находим все символа запроса в порядке их следование в target
    int targetIndex = 0;
    int queryIndex = 0;
    int firstMatchIndex = -1;
    int lastMatchIndex = -1;

    while(queryIndex < query.length() && targetIndex < target.length()) {
        if (query[queryIndex] == target[targetIndex]) {
            if (firstMatchIndex == -1) {
                firstMatchIndex = targetIndex;
            }
            lastMatchIndex = targetIndex;
            queryIndex++;
        }
        targetIndex++;
    }

    // все символы найдены в правильном порядке
    if (queryIndex == query.length()) {
        // рассчитываем балл в зависимости от плотности совпадения
        int matchLength = lastMatchIndex - firstMatchIndex + 1;
        float density = (matchLength > 0) ? static_cast<float>(query.length()) / matchLength : 1.0f;
        int score = config.fuzzySequentialMatchBase + static_cast<int>(density * config.fuzzySequentialDensityScale);
        return qMin(config.fuzzyExactMatchScore, score);
    }

    // частичное совпадение (найдена только часть символов)
    float partialMatchRatio = static_cast<float>(queryIndex) / query.length();
    int score = static_cast<int>(partialMatchRatio * config.fuzzyPartialMatchScale);
    // ограничиваем баллом ниде нечеткого совпадения
    return qMin(config.fuzzySequentialMatchBase > 0 ? config.fuzzySequentialMatchBase - 1 : 0, score);
}

// реализация ContextFIlterStrategy
int ContextFilterStrategy::match(const QString& query, const LspCompletionItem& item, const CompletionScoringConfig& config) const {
    // базовое совпадение с использованием нечеткого поиска
    FuzzyFilterStrategy fuzzy;
    int baseScore = fuzzy.match(query, item, config);

    // учитываем испторию использования
    int usageCount = m_usageCount.value(item.label, 0);
    int usageBonus = qMin(config.contextMaxUsageBonus, usageCount * config.contextUsageBonusScale); // до 20% бонус за частое использование

    // учитываем тип элемента (kind) (функция, переменная и др)
    int kindBonus = 0;
    // значение LSP kind (ПРОВЕРИТЬ !!!!)
    constexpr int LSP_KIND_FUNCTION = 3;
    constexpr int LSP_KIND_METHOD = 2;
    constexpr int LSP_KIND_VARIABLE = 6;
    constexpr int LSP_KIND_FIELD = 5; //поле класса
    constexpr int LSP_KIND_CLASS = 7;
    constexpr int LSP_KIND_INTERFACE = 8;
    constexpr int LSP_KIND_STRUCT = 23;
    constexpr int LSP_KIND_ENUM = 10;
    constexpr int LSP_KIND_KEYWORD = 14;
    switch (item.kind) {
    case LSP_KIND_FUNCTION:
    case LSP_KIND_METHOD:
        kindBonus = config.contextKindBonusFunction;
        break;
    case LSP_KIND_VARIABLE:
    case LSP_KIND_FIELD:
        kindBonus = config.contextKindBonusVariable;
        break;
    case LSP_KIND_CLASS:
    case LSP_KIND_INTERFACE:
    case LSP_KIND_STRUCT:
    case LSP_KIND_ENUM:
        kindBonus = config.contextKindBonusClass;
        break;
    case LSP_KIND_KEYWORD:
        kindBonus = config.contextKindBonusKeyword;
        // Добавляем дополнительный бонус для ключевых слов при полном совпадении
        if (item.label.toLower() == query.toLower()) {
            kindBonus *= 2; // Удваиваем бонус для точного совпадения ключевых слов
        }
        break;
    default:
        kindBonus = 0;
    }

    // TODO: анализ m_currentContext сделать и считать для него бонус и добавлять к итогу

    // комбинируем балл и ограничиваем в 100
    int finalScore =  baseScore + usageBonus + kindBonus;
    return qMin(100, finalScore);
}

void ContextFilterStrategy::addToHistory(const LspCompletionItem& item) {
    m_usageCount[item.label] = m_usageCount.value(item.label, 0) + 1;
}

CompletionWidget::CompletionWidget(QPlainTextEdit* editor, QWidget *parent)
    : QListWidget(parent)
    , m_editor(editor)
    , m_config() // по умолчанию
{
    // setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint); // делаем похожим на всплывашку, стиль без рамки
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_ShowWithoutActivating, true); // не активировать при показе
    setSelectionMode(QAbstractItemView::SingleSelection); // только один элемент может быть выбран
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // горизонтальный скорлл не нужен
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded); // вертикальный по необходимости

    // TODO: добавить загрузку сохраненных настроек

    m_config.normalizeWeights();

    connect(this, &QListWidget::itemDoubleClicked, this, &CompletionWidget::onItemDoubleClicked);
    // сигнал улобне для Enter
    connect(this, &QListWidget::itemActivated, this, &CompletionWidget::triggerSelectionFromItem);

    // инициализация стратегий фильтрации
    m_filterStrategies.push_back(std::make_shared<PrefixFilterStrategy>());
    m_filterStrategies.push_back(std::make_shared<FuzzyFilterStrategy>());
    m_filterStrategies.push_back(std::make_shared<ContextFilterStrategy>());

    // по умолчанию используем нечеткий поиск
    //m_currentStrategy = m_filterStrategies[1];

    // настрйока кжша (макс 50)
    // TODO: доделать кэш, сейчас не используется
    m_filterCache.setMaxCost(50);
}

CompletionWidget::~CompletionWidget() {
    m_filterCache.clear();
    // TODO: при закрытии сохранять настройки
}

void CompletionWidget::setScoringConfig(const CompletionScoringConfig& config) {
    m_config = config;
    m_config.normalizeWeights();
    m_filterCache.clear();
    // TODO: сохранять настройки
}

// загружает данные
void CompletionWidget::updateItems(const QList<LspCompletionItem>& items)
{
    const QSignalBlocker blocker(this); // блок сигналов на время обновления
    clear(); // очищаем старый список
    m_itemData.clear();
    m_originalItems = items; // сохраняем оригинальный список
    m_filterCache.clear();

    // обновляем контекст для улучшения фильтрации
    updateContext();
}

// фильтрует и отображение
bool CompletionWidget::filterItems(const QString& prefix)
{
    qDebug() << "Филтрация по префиксу:" << prefix;
    m_filterCache.clear();

    if (m_originalItems.isEmpty()) {
        hide();
        return false;
    }

    // выполняем фильтрацию СРАЗУ, потому что кэш пока что ненадежен, его нет
    QList<FilterResult> results = performFiltering(prefix);

    // очищаем текущее состояние
    const QSignalBlocker blocker(this);
    clear();
    m_itemData.clear();

    qDebug() << "filterItems: Перед циклом по результатам. Количество:" << results.count();

    for (const FilterResult& result : qAsConst(results)) {
        if (result.score >= m_config.filterThreshold) {
            QListWidgetItem *listItem = new QListWidgetItem(result.lspItem.label);

            // добавление тултуп
            QString tooltip = result.lspItem.detail;
            if (!result.lspItem.documentation.isEmpty()) {
                tooltip += "\n---\n" + result.lspItem.documentation;
            }
            tooltip += QString("\nРелевантность: %1% (%2)").arg(result.score).arg(result.debugInfo);
            listItem->setToolTip(tooltip);

            // TODO: добавить иконку в зависимости от kind

            // заполянем данные
            m_itemData[listItem] = result.lspItem;
            addItem(listItem);
        }
    }

    // показываем или скрываем виджет
    bool shouldShow = (count() > 0);
    if (shouldShow) {
        setCurrentRow(0);
        scrollToItem(currentItem(), QAbstractItemView::PositionAtTop);
        if (!isVisible()) {
            show();
        }
        adjustSize(); // подгоняем размер после добавления элементов
    } else {
        hide();
    }

    return shouldShow;
}

void CompletionWidget::focusOutEvent(QFocusEvent *event)
{
    //hide(); // скрываем, если кликнули мимо
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
            result.debugInfo = "Пустой запрос";
            results.append(result);
        }
        std::sort(results.begin(), results.end());
        return results;
    }

    QString lowerQuery = query.toLower();

    // фильтруем с использованием выбранной стратегии
    for (const auto& item : m_originalItems) {
        int bestMatchQuality = 99; //худшее качество по умолчанию
        int currentPrefixScore = 0;
        int currentFuzzyScore = 0;
        int currentContextScore = 0;
        int qualityBonus = 0; // бонус к баллу за качество

        // ОПРЕДЕЛЕНИЕ КАЧЕСТВА СОВПАДЕНИЯ
        // сначала проверяем с учетом регистра для более высокого приоритета
        bool isExactMatch = false;
        bool isKeyword = (item.kind == 14); // LSP_KIND_KEYWORD = 14
        
        if (item.label.compare(query, Qt::CaseSensitive) == 0) { //compare для точности
            bestMatchQuality = 0;
            qualityBonus = 30; // max
            isExactMatch = true;
        } else if (item.label.startsWith(query, Qt::CaseSensitive)) {
            bestMatchQuality = 1;
            qualityBonus = 25;
        }
        // без учета регистра
        else if (item.label.compare(query, Qt::CaseInsensitive) == 0) { // для регистронезависимости
            bestMatchQuality = 2;
            qualityBonus = 20;
            isExactMatch = true;
        } else if (item.label.startsWith(lowerQuery, Qt::CaseInsensitive)) { // startWith без учета регистра
            bestMatchQuality = 3;
            qualityBonus = 15;
        }
        // проверка начала слова
        else if (isWordStartMatch(query, item.label)) {
            bestMatchQuality = 4;
            qualityBonus = 10;
        }
        
        // Дополнительный бонус для ключевых слов при точном совпадении
        if (isKeyword && isExactMatch) {
            bestMatchQuality = 0; // Высший приоритет для точного совпадения ключевых слов
            qualityBonus += 30;   // Добавляем дополнительный бонус для ключевых слов
        }
        // Бонус для ключевых слов при префиксном совпадении
        else if (isKeyword && (bestMatchQuality <= 3)) {
            qualityBonus += 15;   // Добавляем бонус для ключевых слов с префиксным совпадением
        }

        // получение баллов от стратегий
        // TODO: если одна дала 0, то другие и не проверяем, ОПТИМИЗАЦИЯ
        for (const auto& strategy : m_filterStrategies) {
            int score = strategy->match(query, item, m_config);
            // qDebug() << "   Item:" << item.label << "Strategy:" << strategy->name() << "Raw Score:" << score;

            if (strategy->name() == "Префикс") {
                currentPrefixScore = score;
                // если префиксный балл высокий, но качество не определено как префиксное - то уточняем
                if (score >= m_config.prefixStartMatchBase && bestMatchQuality > 3) {
                    bestMatchQuality = 3; // как минимум регистронезависимый префикс
                } else if (score >= m_config.prefixWordStartMatchScore && bestMatchQuality > 4) {
                    bestMatchQuality = 4; // как минимум начало слова
                } else if (score >= m_config.prefixContainsBase && bestMatchQuality > 20) {
                    bestMatchQuality = 20; // как миниму содержит (низкий приоритет)
                }
            } else if (strategy->name() == "Нечеткая фильтрация") {
                currentFuzzyScore = score;
                // если есть нечеткий балл, а качество дефолт все ещё, то ставим нечеткое качество
                if (score > 0 && bestMatchQuality > 10) {
                    bestMatchQuality = 10;
                }
            } else if (strategy->name() == "Контекстный") {
                currentContextScore = score;
            }
        }

        // вычисляем взвешенный общий балл
        float combinedScore =
            m_config.prefixWeight * currentPrefixScore + // Теперь переменные объявлены
            m_config.fuzzyWeight * currentFuzzyScore +   // и им присвоены значения
            m_config.contextWeight * currentContextScore;

        // добавляем бонус за определенное раннее качество
        int finalScore = qMin(100, qRound(combinedScore) + qualityBonus);
        
        // Специальная обработка для ключевых слов при близком совпадении
        if (isKeyword && item.label.length() <= 8 && query.length() >= 2 && 
            (item.label.startsWith(query, Qt::CaseInsensitive) || 
             query.length() >= item.label.length() * 0.7)) {
            finalScore = qMin(100, finalScore + 10); // Дополнительный бонус для коротких ключевых слов
        }
        
        // Отладка итогового балла
        qDebug() << "    -> Final Score:" << finalScore
                 << QString(" (P:%1*%.1f + F:%2*%.1f + C:%3*%.1f + QB:%4))")
                        .arg(currentPrefixScore).arg(m_config.prefixWeight)
                        .arg(currentFuzzyScore).arg(m_config.fuzzyWeight)
                        .arg(currentContextScore).arg(m_config.contextWeight).arg(qualityBonus);

        // если итоговый балл выше порого, то рез добавляем
        if (finalScore >= m_config.filterThreshold) {
            FilterResult result;
            result.lspItem = item;
            result.item = nullptr;
            result.score = finalScore;
            result.matchQuality = bestMatchQuality;

            // отладочная инфа с сохранением отдельных баллов
            result.debugInfo = QString("Q:%1 P:%2 N:%3 K:%4 QB:%5").arg(currentPrefixScore).arg(currentFuzzyScore).arg(currentContextScore).arg(qualityBonus);
            // Отладочная инфа с сохранением отдельных баллов
            results.append(result);
            qDebug() << "      --> ADDED:" << item.label << "Score:" << finalScore << "Quality:" << bestMatchQuality;
        }
    }

    // сортируем результуты
    std::sort(results.begin(), results.end());
    qDebug() << "performFiltering: Завершено. Найдено результатов:" << results.count() << " для запроса:" << query;
    return results;
}

// вспомогательная функция для поиска начала слова (Camel/snake)
bool CompletionWidget::isWordStartMatch(const QString& query, const QString& label) {
    if (query.isEmpty() || label.isEmpty()) {
        return false;
    }

    QString lowerQuery = query.toLower();
    QStringList words;
    QString currentWord;

    // разбор label на слова (как в PrefixFilterStrategy)
    for (int i = 0; i < label.length(); ++i) {
        QChar c = label[i];
        if (c.isUpper() && i > 0) {
            if (!currentWord.isEmpty()) words.append(currentWord);
            currentWord = c;
        } else if (c == '_') {
            if (!currentWord.isEmpty()) words.append(currentWord);
            currentWord.clear();
        } else {
            currentWord += c;
        }
    }
    if (!currentWord.isEmpty()) {
        words.append(currentWord);
    }

    // проверка начинается ли какое либо слово (без регистра)
    for (const QString& w : qAsConst(words)) {
        if (w.startsWith(lowerQuery, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
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
    // --------------- БОЛЬШЕ НЕ ИСПОЛЬЗУЕТСЯ
    // for (const auto& strategy : m_filterStrategies) {
    //     if (strategy->name() == strategyName) {
    //         m_currentStrategy = strategy;
    //         // кэш при смене стратгеии очищаем
    //         m_filterCache.clear();
    //         return;
    //     }
    // }

    // // если стратегия не найдена, то используем нечеткий поиск по умолчанию
    // qDebug() << "Стратегия не найдена:" << strategyName << ".Используется нечеткий поиск по умолчанию";
    // m_currentStrategy = m_filterStrategies[1];
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
                break;
            }
        }

        emit completionSelected(text); // отправляем текст вставки
        hide();

    } else {
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

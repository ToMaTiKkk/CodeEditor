// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMPLETIONWIDGET_H
#define COMPLETIONWIDGET_H

#include <QListWidget>
#include "lspmanager.h" // нужнео для LspCompletionItem
#include <QPlainTextEdit>
#include <QHash>
#include <QCache>
#include <memory>
#include <functional>
#include <QSettings>
#include <limits> // для std::numeric_limits

// конфиг оценки
struct CompletionScoringConfig {
    // общие
    int filterThreshold = 50; // минимальный порог для показа элемента (0-100)

    // веса дял стратегий фильтрации (в сумме 1.0)
    float prefixWeight = 0.4f; // увеличил с 0.3 до 0.4 - больше влияния префиксной фильтрации
    float fuzzyWeight = 0.3f; // уменьшил с 0.4 до 0.3
    float contextWeight = 0.3f; // контекстной стратегии

    // настройки PrefixFilterStrategy
    int prefixExactMatchScore = 100;
    int prefixStartMatchBase = 90; // увеличил с 85 до 90 - выше базовый балл за начало
    int prefixStartLengthBonusScale = 15; // макс бонус = scale * len / target_len
    int prefixContainsBase = 40; // базовый балл за содержание (не с начала)
    int prefixContainsPosPenaltyScale = 30; // штраф за позицию = scale * pos / target_len (макс)
    int prefixWordStartMatchScore = 65; // балл за совпадение с началом слова (camel/snake)
    int prefixMinScoreIfContains = 10; // минимальный балл если просто содержит

    // настройки FuzzyFilterStrategy
    int fuzzyExactMatchScore = 100;
    int fuzzyPrefixMatchBase = 90; // балл за начало + бонус за длину
    int fuzzyPrefixLengthBonusScale = 10;
    int fuzzyContainsBase = 75; // балл за содержание (подстрока)
    int fuzzyContainsLengthBonusScale = 10;
    int fuzzySequentialMatchBase = 50; // базовый балл за нечеткое последоват совпадение
    int fuzzySequentialDensityScale = 45; // бонус за плотность = scale * density
    int fuzzyPartialMatchScale = 40; // балл за частичное нечеткое совпадение = scale * ratio
    float fuzzyDocMatchMultiplier = 0.5f; // множитель для баллов из документации
    float fuzzyDetailMatchMultiplier = 0.3f; // множетель для баллов из деталей

    // настройки ContextFilterStrategy
    int contextUsageBonusScale = 5; // балл за каждое использование (5 * count)
    int contextMaxUsageBonus = 25; // максимальный бонус за использование
    int contextKindBonusFunction = 15; // бонус для функций и методов
    int contextKindBonusVariable = 5; // бонус для переменных и полей
    int contextKindBonusClass = 10; // бонус для классов, структур, интерфейсов
    int contextKindBonusKeyword = 20; // увеличил с 8 до 20 - больший бонус для ключевых слов
    // TODO: добавить штрафы и бонусы за совпадение типов, область видимости и тп

    // метод для нормализации весов чтобы сумма было 1.0
    void normalizeWeights() {
        float sum = prefixWeight + fuzzyWeight + contextWeight;
        if (sum > std::numeric_limits<float>::epsilon()) { // проверка на не нулевую сумму
            prefixWeight /= sum;
            fuzzyWeight /= sum;
            contextWeight /= sum;
        } else {
            // устанавливаем веса по умолчанию, если сумма 0
            prefixWeight = 1.0f / 3.0f;
            fuzzyWeight = 1.0f / 3.0f;
            contextWeight = 1.0f / 3.0f;
        }
    }

    // сохранение и загрузка
    void save(QSettings& settings, const QString& group = "Completion/Scoring") {
        settings.beginGroup(group);

        // общие
        settings.setValue("filterThreshold", filterThreshold);
        settings.setValue("prefixWeight", prefixWeight);
        settings.setValue("fuzzyWeight", fuzzyWeight);
        settings.setValue("contextWeight", contextWeight);

        // настройки PrefixFilterStrategy
        settings.setValue("prefixExactMatchScore", prefixExactMatchScore);
        settings.setValue("prefixStartMatchBase", prefixStartMatchBase);
        settings.setValue("prefixStartLengthBonusScale", prefixStartLengthBonusScale);
        settings.setValue("prefixContainsBase", prefixContainsBase);
        settings.setValue("prefixContainsPosPenaltyScale", prefixContainsPosPenaltyScale);
        settings.setValue("prefixWordStartMatchScore", prefixWordStartMatchScore);
        settings.setValue("prefixMinScoreIfContains", prefixMinScoreIfContains);

        // настройки FuzzyFilterStrategy
        settings.setValue("fuzzyExactMatchScore", fuzzyExactMatchScore);
        settings.setValue("fuzzyPrefixMatchBase", fuzzyPrefixMatchBase);
        settings.setValue("fuzzyPrefixLengthBonusScale", fuzzyPrefixLengthBonusScale);
        settings.setValue("fuzzyContainsBase", fuzzyContainsBase);
        settings.setValue("fuzzyContainsLengthBonusScale", fuzzyContainsLengthBonusScale);
        settings.setValue("fuzzySequentialMatchBase", fuzzySequentialMatchBase);
        settings.setValue("fuzzySequentialDensityScale", fuzzySequentialDensityScale);
        settings.setValue("fuzzyPartialMatchScale", fuzzyPartialMatchScale);
        settings.setValue("fuzzyDocMatchMultiplier", fuzzyDocMatchMultiplier);
        settings.setValue("fuzzyDetailMatchMultiplier", fuzzyDetailMatchMultiplier);

        // настройки ContextFilterStrategy
        settings.setValue("contextUsageBonusScale", contextUsageBonusScale);
        settings.setValue("contextMaxUsageBonus", contextMaxUsageBonus);
        settings.setValue("contextKindBonusFunction", contextKindBonusFunction);
        settings.setValue("contextKindBonusVariable", contextKindBonusVariable);
        settings.setValue("contextKindBonusClass", contextKindBonusClass);
        settings.setValue("contextKindBonusKeyword", contextKindBonusKeyword);

        settings.endGroup();
    }

    void load(QSettings& settings, const QString& group = "Completion/Scoring") {
        settings.beginGroup(group);

        // общие
        filterThreshold = settings.value("filterThreshold", filterThreshold).toInt();
        prefixWeight = settings.value("prefixWeight", prefixWeight).toFloat();
        fuzzyWeight = settings.value("fuzzyWeight", fuzzyWeight).toFloat();
        contextWeight = settings.value("contextWeight", contextWeight).toFloat();

        // настройки PrefixFilterStrategy
        prefixExactMatchScore = settings.value("prefixExactMatchScore", prefixExactMatchScore).toInt();
        prefixStartMatchBase = settings.value("prefixStartMatchBase", prefixStartMatchBase).toInt();
        prefixStartLengthBonusScale = settings.value("prefixStartLengthBonusScale", prefixStartLengthBonusScale).toInt();
        prefixContainsBase = settings.value("prefixContainsBase", prefixContainsBase).toInt();
        prefixContainsPosPenaltyScale = settings.value("prefixContainsPosPenaltyScale", prefixContainsPosPenaltyScale).toInt();
        prefixWordStartMatchScore = settings.value("prefixWordStartMatchScore", prefixWordStartMatchScore).toInt();
        prefixMinScoreIfContains = settings.value("prefixMinScoreIfContains", prefixMinScoreIfContains).toInt();

        // настройки FuzzyFilterStrategy
        fuzzyExactMatchScore = settings.value("fuzzyExactMatchScore", fuzzyExactMatchScore).toInt();
        fuzzyPrefixMatchBase = settings.value("fuzzyPrefixMatchBase", fuzzyPrefixMatchBase).toInt();
        fuzzyPrefixLengthBonusScale = settings.value("fuzzyPrefixLengthBonusScale", fuzzyPrefixLengthBonusScale).toInt();
        fuzzyContainsBase = settings.value("fuzzyContainsBase", fuzzyContainsBase).toInt();
        fuzzyContainsLengthBonusScale = settings.value("fuzzyContainsLengthBonusScale", fuzzyContainsLengthBonusScale).toInt();
        fuzzySequentialMatchBase = settings.value("fuzzySequentialMatchBase", fuzzySequentialMatchBase).toInt();
        fuzzySequentialDensityScale = settings.value("fuzzySequentialDensityScale", fuzzySequentialDensityScale).toInt();
        fuzzyPartialMatchScale = settings.value("fuzzyPartialMatchScale", fuzzyPartialMatchScale).toInt();
        fuzzyDocMatchMultiplier = settings.value("fuzzyDocMatchMultiplier", fuzzyDocMatchMultiplier).toFloat();
        fuzzyDetailMatchMultiplier = settings.value("fuzzyDetailMatchMultiplier", fuzzyDetailMatchMultiplier).toFloat();

        // настройки ContextFilterStrategy
        contextUsageBonusScale = settings.value("contextUsageBonusScale", contextUsageBonusScale).toInt();
        contextMaxUsageBonus = settings.value("contextMaxUsageBonus", contextMaxUsageBonus).toInt();
        contextKindBonusFunction = settings.value("contextKindBonusFunction", contextKindBonusFunction).toInt();
        contextKindBonusVariable = settings.value("contextKindBonusVariable", contextKindBonusVariable).toInt();
        contextKindBonusClass = settings.value("contextKindBonusClass", contextKindBonusClass).toInt();
        contextKindBonusKeyword = settings.value("contextKindBonusKeyword", contextKindBonusKeyword).toInt();

        settings.endGroup();
        normalizeWeights(); // пересчитать веса после загрузки на случай, если они не нормализованы в настройках
    }
};

// классы абстрактные для стратегий фильтрации
class IFilterStrategy {
public:
    virtual ~IFilterStrategy() = default;
    // основной метод фильтрации, возвращает значение Score от 0 до 100, где 100-макс
    virtual int match(const QString& query, const LspCompletionItem& item, const CompletionScoringConfig& config) const = 0;
    // название стратегии
    virtual QString name() const = 0;
};

// простая префиксная фильтрация
class PrefixFilterStrategy : public IFilterStrategy {
public:
    int match(const QString& query, const LspCompletionItem& item, const CompletionScoringConfig& config) const override;
    QString name() const override { return "Префикс"; }
};

// более продвинутая нечеткая фильтрация
class FuzzyFilterStrategy : public IFilterStrategy {
public:
    int match(const QString& query, const LspCompletionItem& item, const CompletionScoringConfig& config) const override;
    QString name() const override { return "Нечеткая фильтрация"; }

private:
    // вспомогательный метод для расчета очков нечеткого соответствия
    int calculateFuzzyScore(const QString& query, const QString& target, const CompletionScoringConfig& config) const;
};

// Контекстаня фильтрация с учетом частоты использования
class ContextFilterStrategy : public IFilterStrategy {
public:
    int match(const QString& query, const LspCompletionItem& item, const CompletionScoringConfig& config) const override;
    QString name() const override { return "Контекстный"; }

    // добавляет информацию о выбранном элементе для улучшения будущих резултатов
    void addToHistory(const LspCompletionItem& item);

private:
    mutable QHash<QString, int> m_usageCount; // частота использования элементов
};

// для результато вфильтрации
struct FilterResult {
    QListWidgetItem* item;
    LspCompletionItem lspItem;
    int score; // 0-100
    // Качество совпадения (чем НИЖЕ, тем ЛУЧШЕ)
    // 0: Точное совпадение label == query (case-sensitive)
    // 1: Точное совпадение префикса (startsWith, case-sensitive)
    // 2: Точное совпадение label == query (case-insensitive)
    // 3: Точное совпадение префикса (startsWith, case-insensitive)
    // 4: Совпадение с началом слова (Camel/snake, case-insensitive)
    // 10: Нечеткое совпадение (Fuzzy Sequential)
    // 20: Содержит как подстроку (Contains, case-insensitive) - Низкий приоритет
    // 99: Другое / Не определено
    int matchQuality;
    QString debugInfo; // для отображения отдельных оценок

    // сортировка: сначала по качеству (возрастание), потом по баллу (убывание)
    bool operator<(const FilterResult& other) const {
        // Сначала обрабатываем особые случаи с ключевыми словами
        // Если текущий элемент - точное совпадение ключевого слова (void == void), а другой нет
        if (lspItem.kind == 14 && matchQuality <= 2 && 
            (other.lspItem.kind != 14 || other.matchQuality > 2)) {
            return true; // Точное совпадение ключевого слова всегда выше
        }
        
        // Если другой элемент - точное совпадение ключевого слова, а текущий нет
        if (other.lspItem.kind == 14 && other.matchQuality <= 2 && 
            (lspItem.kind != 14 || matchQuality > 2)) {
            return false; // Другой элемент имеет преимущество
        }
        
        // Стандартная сортировка по качеству
        if (matchQuality != other.matchQuality) {
            return matchQuality < other.matchQuality; // лучшее качество идет первым
        }
        
        // Дополнительное сравнение по типу при одинаковом качестве
        bool thisIsKeyword = (lspItem.kind == 14);
        bool otherIsKeyword = (other.lspItem.kind == 14);
        
        if (thisIsKeyword && !otherIsKeyword) {
            return true; // ключевые слова имеют приоритет
        }
        if (!thisIsKeyword && otherIsKeyword) {
            return false;
        }
        
        // При одинаковом качестве и типе, больший балл идет первым
        if (score != other.score) {
            return score > other.score;
        }
        
        // Для одинаковых баллов учитываем длину (более короткие имеют преимущество)
        if (lspItem.label.length() != other.lspItem.label.length()) {
            return lspItem.label.length() < other.lspItem.label.length();
        }
        
        // если и качество и балл и длина равны, сортируем по label для стабильности
        return lspItem.label < other.lspItem.label;
    }
};

class CompletionWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit CompletionWidget(QPlainTextEdit* editor, QWidget *parent = nullptr);
    ~CompletionWidget();

    // метод для установки нового конфига
    void setScoringConfig(const CompletionScoringConfig& config);
    const CompletionScoringConfig& scoringConfig() const { return m_config; }

    void updateItems(const QList<LspCompletionItem>& items); // заполнить список
    void triggerSelection(); //выбрать текущий элемент (по Enter/Tab)
    void triggerSelectionFromItem(QListWidgetItem *item); // ывбрать конкретный

    bool filterItems(const QString& prefix); // метод для фильтрации
    QStringList availableFilterStrategies() const;

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

    // система фильтрации
    std::vector<std::shared_ptr<IFilterStrategy>> m_filterStrategies;
    std::shared_ptr<IFilterStrategy> m_currentStrategy;

    // кэш результатов фильтрации
    QCache<QString, QList<FilterResult>> m_filterCache;
    // контекстная информация для улучшения фильтрации
    QString m_currentContext; // текст перед курсором для контекста, его анализа
    // метод для выполнения филтрации с текущей стратегией
    QList<FilterResult> performFiltering(const QString& query); // берем m_config
    // метод для обновления контекстной информации
    void updateContext();

    CompletionScoringConfig m_config; // храним текущий конфиг

    int findNextVisibleRow(int startRow, int step); // найти следующий или предыдущий видимый
    bool isWordStartMatch(const QString& query, const QString& label);
};

#endif

#ifndef COMPLETIONWIDGET_H
#define COMPLETIONWIDGET_H

#include <QListWidget>
#include "lspmanager.h" // нужнео для LspCompletionItem
#include <QPlainTextEdit>
#include <QHash>
#include <QCache>
#include <memory>
#include <functional>

// классы абстрактные для стратегий фильтрации
class IFilterStrategy {
public:
    virtual ~IFilterStrategy() = default;
    // основной метод фильтрации, возвращает значение Score от 0 до 100, где 100-макс
    virtual int match(const QString& query, const LspCompletionItem& item) const = 0;
    // название стратегии
    virtual QString name() const = 0;
};

// простая префиксная фильтрация
class PrefixFilterStrategy : public IFilterStrategy {
public:
    int match(const QString& query, const LspCompletionItem& item) const override;
    QString name() const override { return "Префикс"; }
};

// более продвинутая нечеткая фильтрация
class FuzzyFilterStrategy : public IFilterStrategy {
public: 
    int match(const QString& query, const LspCompletionItem& item) const override;
    QString name() const override { return "Нечеткая фильтрация"; } 

private:
    // вспомогательный метод для расчета очков нечеткого соответствия
    int calculateFuzzyScore(const QString& query, const QString& target) const;
};

// Контекстаня фильтрация с учетом частоты использования
class ContextFilterStrategy : public IFilterStrategy {
public:
    int match(const QString& query, const LspCompletionItem& item) const override;
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
    QString debugInfo; // для отображения отдельных оценок

    bool operator<(const FilterResult& other) const {
        return score > other.score; // для сортировки по убыванию
    }
};

class CompletionWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit CompletionWidget(QPlainTextEdit* editor, QWidget *parent = nullptr);
    ~CompletionWidget();

    void updateItems(const QList<LspCompletionItem>& items); // заполнить список
    void triggerSelection(); //выбрать текущий элемент (по Enter/Tab)
    void triggerSelectionFromItem(QListWidgetItem *item); // ывбрать конкретный

    void filterItems(const QString& prefix); // метод для фильтрации
    void setFilterThreshold(int threshold) { m_filterThreshold = threshold; }
    void setCurrentFilterStrategy(const QString& strategyName);
    QStringList availableFilterStrategies() const;

    // методы внешнего управления
    void navigateUp();
    void navigateDown();
    void navigatePageUp();
    void navigatePageDown();

    // метод для настрйоки весов
    void setStrategyWeights(float prefix, float fuzzy, float context) {
        float sum = prefix + fuzzy + context;
        if (sum > 0) {
            // нормализуем веса, чтобы сумма была равна 1
            m_prefixWeight = prefix / sum;
            m_fuzzyWeight = fuzzy / sum;
            m_contextWeight = context / sum;
        }
    }

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
    int m_filterThreshold = 50; // минимальный порог для показа элемента (0-100)
    // кэш результатов фильтрации
    QCache<QString, QList<FilterResult>> m_filterCache;
    // контекстная информация для улучшения фильтрации
    QString m_currentContext; // текст перед курсором для контекста, его анализа
    // метод для выполнения филтрации с текущей стратегией
    QList<FilterResult> performFiltering(const QString& query);
    // метод для обновления контекстной информации
    void updateContext();

    // веса дял стратегий фильтрации (в сумме 1.0)
    float m_prefixWeight = 0.3f; // 30% влияний префиксной фильтрации
    float m_fuzzyWeight = 0.4f; // для нечеткйо стратегии
    float m_contextWeight = 0.3f; // контекстной стратегии

    int findNextVisibleRow(int startRow, int step); // найти следующий или предыдущий видимый
};

#endif

#ifndef DOCUMENTMODEL_H
#define DOCUMENTMODEL_H

#include <QObject>
#include <QString>
#include <QVector>

// DocumentModel - это сердце нашего виртуального редактора.
// Он хранит все строки документа, но не занимается их отображением.
// Он предоставляет интерфейс для доступа к данным и их изменения.
// Это разделение данных (Model) и представления (View) - ключевой архитектурный принцип.
class DocumentModel : public QObject
{
    Q_OBJECT

public:
    explicit DocumentModel(QObject *parent = nullptr);

    // --- Интерфейс для доступа к данным ---
    int lineCount() const;
    QString lineAt(int index) const;
    const QVector<QString>& allLines() const; // Для сохранения и начальной загрузки

    // --- Интерфейс для изменения данных ---
    void insertLine(int index, const QString& line);
    void removeLine(int index);
    void replaceLine(int index, const QString& line);
    void setContent(const QString& content); // Установить всё содержимое из одной большой строки
    void appendChunk(const QString& chunk); // Добавление данных "кусками"
    void clear();

signals:
    // Сигнал, который мы будем отправлять, когда документ изменится.
    // Это позволит нашему редактору и другим системам (LSP!) узнать, что нужно обновиться.
    void documentChanged();

private:
    QVector<QString> m_lines; // Храним документ как вектор строк. Это очень эффективно для доступа по номеру строки.
};

#endif // DOCUMENTMODEL_H

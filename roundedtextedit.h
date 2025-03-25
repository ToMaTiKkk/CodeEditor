#ifndef ROUNDEDTEXTEDIT_H
#define ROUNDEDTEXTEDIT_H

#include <QTextEdit>
#include <QPainter>
#include <QTextFrame>
#include <QScrollBar>
#include <QPainterPath>
#include <QAbstractTextDocumentLayout>

class RoundedTextEdit : public QTextEdit {
    Q_OBJECT
public:
    explicit RoundedTextEdit(QWidget *parent = nullptr);

    // Добавляем объявление метода
    void setUsername(const QString &username);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_username;  // Добавляем переменную для хранения имени пользователя
};

#endif // ROUNDEDTEXTEDIT_H

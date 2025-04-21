#ifndef CODEPLAINTEXTEDIT_H
#define CODEPLAINTEXTEDIT_H

#include <QKeyEvent>
#include <QTextCursor>
#include <QPlainTextEdit>

// класс для автоскобок и
class CodePlainTextEdit : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit CodePlainTextEdit(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

signals:
    void completionShortcut(); // при crtl+space
    void definitionnShortcut(); // F12
};

#endif //CODEPLAINTEXTEDIT_H

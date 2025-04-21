#include "codeplaintextedit.h"
#include <QTextCursor>
#include <QTextBlock>

CodePlainTextEdit::CodePlainTextEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
}

void CodePlainTextEdit::keyPressEvent(QKeyEvent *event)
{
    // горячие клавиши
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Space) {
        emit completionShortcut();
        return;
    }
    if (event->key() == Qt::Key_F12) {
        emit definitionnShortcut();
        return;
    }

    QString keyText = event->text();
    // авто скобка, вставляем пару
    if (keyText == "(" || keyText == "[" || keyText == "{" || keyText == "\"" || keyText == "'") {
        QChar closing = (keyText == "(" ? ')' :  keyText == "[" ? ']' : keyText == "{" ? '}' : keyText == "\"" ? '"' : '\'');
        QTextCursor cursor = textCursor();
        if (keyText == "{") {
            // для фигурных шаблон с новый строки
            QString block = cursor.block().text();
            QString indent;
            for (QChar c : block) {
                if (c.isSpace()) {
                    indent += c;
                } else break;
            }
            QString ins = "{\n" + indent + "\t\n" + indent + "}";
            cursor.insertText(ins);
            cursor.movePosition(QTextCursor::PreviousBlock);
            cursor.movePosition(QTextCursor::EndOfLine);
        } else {
            cursor.insertText(keyText + closing);
            cursor.movePosition(QTextCursor::PreviousCharacter);
        }
        setTextCursor(cursor);
        return;
    }

    // пропускаем уже введенные элементы
    if (keyText == ")" || keyText == "]" || keyText == "}" || keyText == "\"" || keyText == "'") {
        QTextCursor cursor = textCursor();
        int pos = cursor.position();
        if (pos < document()->characterCount() && document()->characterAt(pos) == keyText.at(0)) {
            cursor.movePosition(QTextCursor::NextCharacter);
            setTextCursor(cursor);
            return;
        }
    }

    // остальное в стандартную обработку
    QPlainTextEdit::keyPressEvent(event);
}

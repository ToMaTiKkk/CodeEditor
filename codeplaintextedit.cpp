// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#include "codeplaintextedit.h"
#include <QTextCursor>
#include <QTextBlock>
#include <QScrollBar>
#include <QPropertyAnimation>
#include <QEasingCurve>

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

    if (event->key() == Qt::Key_Tab) {
        const QString tab = "    ";
        textCursor().insertText(tab);
        return;
    }

    QString keyText = event->text();
    // авто скобка, вставляем пару
    if (keyText == "(" || keyText == "[" || keyText == "{" || keyText == "\"" || keyText == "'") {
        QChar closing = (keyText == "(" ? ')' :  keyText == "[" ? ']' : keyText == "{" ? '}' : keyText == "\"" ? '"' : '\'');
        QTextCursor cursor = textCursor();
        cursor.insertText(keyText + closing);
        cursor.movePosition(QTextCursor::PreviousCharacter);

        setTextCursor(cursor);
        return;
    }

    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        QTextCursor cursor = textCursor();

        int position = cursor.position();

        // проверка символа перед курсором
        if (position > 0) {
            QChar prevChar = document()->characterAt(position - 1);

            // проверка после курсора
            if (position < document()->characterCount()) {
                QChar nextChar = document()->characterAt(position);

                if (prevChar == "{" && nextChar == "}") {
                    cursor.beginEditBlock();

                    // вычисляем отступ начала след строки
                    QString block = cursor.block().text();
                    QString baseIndent;
                    for (QChar c : block) {
                        if (c.isSpace()) {
                            baseIndent += c;
                        } else break;
                    }
                    const QString tab = "    ";
                    QString ins = "\n" + baseIndent + tab + "\n" + baseIndent;
                    cursor.insertText(ins);

                    // ставим курсор на позицию для кода
                    cursor.movePosition(QTextCursor::Up);
                    cursor.movePosition(QTextCursor::EndOfLine);

                    setTextCursor(cursor);
                    cursor.endEditBlock();
                    return;
                }
            }
        }

        // если это не блок {}, то просто сохраняем отступ
        QString currentLineText = cursor.block().text();
        QString indentation;

        // получение текущего отступа
        for (QChar c : currentLineText) {
            if (c.isSpace()) {
                indentation += c;
            } else break;
        }

        // вставаляем новую стркоу с текущим отступом
        cursor.beginEditBlock();
        cursor.insertText("\n" + indentation);
        cursor.endEditBlock();
        return;
    }

    // удаление отступов
    if (event->key() == Qt::Key_Backspace) {
        QTextCursor cursor = textCursor();
        QString currentLineText = cursor.block().text();
        int cursorPosInLine = cursor.positionInBlock();

        // проверяем где находится курсор, в облатси отступа или нет
        bool onlySpaceBeforeCursor = true;
        for (int i = 0; i < cursorPosInLine; i++) {
            if (!currentLineText[i].isSpace()) {
                onlySpaceBeforeCursor = false;
                break;
            }
        }

        // если перед курсором только пробелы и там есть место для удаления
        if (onlySpaceBeforeCursor && cursorPosInLine > 0) {
            const int tabSize = 4;
            int spaceToRemove;

            // удаляем 4 пробела, но не больше
            spaceToRemove = std::min(cursorPosInLine, tabSize);

            // если количество пробелов не кратно tabSize, удаляем до ближайшего кратного
            int remainder = cursorPosInLine & tabSize;
            if (remainder > 0) {
                spaceToRemove = remainder;
            }

            // удаляем блок отступа
            cursor.beginEditBlock();
            for (int i = 0; i < spaceToRemove; i++) {
                cursor.deletePreviousChar();
            }
            cursor.endEditBlock();
            return;
        }
    }

    // пропускаем уже введенные элементы
    if (keyText == ")" || keyText == "]" || keyText == "}" || keyText == "\"" || keyText == "'" || keyText == '"') {
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

void CodePlainTextEdit::wheelEvent(QWheelEvent* event)
{
    int deltaY_pixels = 0;

    // Для высокоточных устройств (тачпады)
    if (!event->pixelDelta().isNull()) {
        // если pixelDelta().y() > 0 (скролл вверх), то scrollbar->value() должен уменьшиться
        deltaY_pixels = event->pixelDelta().y();
    }
    // Для обычной мыши
    else if (!event->angleDelta().isNull()) {
        // angleDelta().y() > 0 (колесико от себя) означает скролл контента ВВЕРХ
        double numTicks = event->angleDelta().y() / 120.0;

        // количество строк на один "тик" колеса
        double linesPerTick = 1.1; // Полстроки за тик

        deltaY_pixels = static_cast<int>(numTicks * linesPerTick * fontMetrics().height());
    }

    if (deltaY_pixels != 0) {
        // (положительное значение -> скролл вверх).
        smoothScrollBy(deltaY_pixels);
    }
    event->accept();
}

void CodePlainTextEdit::smoothScrollBy(int deltaY_pixels)
{
    QScrollBar *vs = verticalScrollBar();
    int startValue = vs->value();
    int endValue = startValue - deltaY_pixels;

    // Ограничиваем конец допустимым диапазоном
    endValue = qBound(vs->minimum(), endValue, vs->maximum());

    auto *anim = new QPropertyAnimation(vs, "value", this);
    anim->setDuration(150); // чем меньше, тем быстрее, а иначе = более тяжелая инерция
    anim->setStartValue(startValue); // начинаем с текущего реального значения
    anim->setEndValue(endValue);
    anim->setEasingCurve(QEasingCurve::OutCubic); // сглаженная кривая
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

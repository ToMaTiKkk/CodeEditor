// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CPPHIGHLIGHTER_H
#define CPPHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QColor>
#include <QTextDocument>

class CppHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit CppHighlighter(QTextDocument *document, const QString &filePath, QObject *parent = nullptr);
    bool isSupportedFileSuffix(const QString &fileName) const;

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat quolationFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat preprocessorFormat;
    QString currentFilePath;
    QRegularExpression commentStartExpression;
    QRegularExpression commentEndExpression;
};

#endif

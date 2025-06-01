// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cpphighlighter.h"
#include <utility> // добавил для использования std::as_const() из C++17
#include <QFileInfo>


CppHighlighter::CppHighlighter(QTextDocument *document, const QString &filePath, QObject *parent)
    : QSyntaxHighlighter(document),
    currentFilePath(filePath)
{
    HighlightingRule rule;

    keywordFormat.setForeground(QColor(161, 134, 255));
    keywordFormat.setFontWeight(QFont::Bold);
    const QString keywordPatterns[] = {
        QStringLiteral("\\bchar\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bconst\\b"),
        QStringLiteral("\\bdouble\\b"), QStringLiteral("\\benum\\b"), QStringLiteral("\\bexplicit\\b"),
        QStringLiteral("\\bfriend\\b"), QStringLiteral("\\binline\\b"), QStringLiteral("\\bint\\b"),
        QStringLiteral("\\blong\\b"), QStringLiteral("\\bnamespace\\b"), QStringLiteral("\\boperator\\b"),
        QStringLiteral("\\bprivate\\b"), QStringLiteral("\\bprotected\\b"), QStringLiteral("\\bpublic\\b"),
        QStringLiteral("\\bshort\\b"), QStringLiteral("\\bsignals\\b"), QStringLiteral("\\bsigned\\b"),
        QStringLiteral("\\bslots\\b"), QStringLiteral("\\bstatic\\b"), QStringLiteral("\\bstruct\\b"),
        QStringLiteral("\\btemplate\\b"), QStringLiteral("\\btypedef\\b"), QStringLiteral("\\btypename\\b"),
        QStringLiteral("\\bunion\\b"), QStringLiteral("\\bunsigned\\b"), QStringLiteral("\\bvirtual\\b"),
        QStringLiteral("\\bvoid\\b"), QStringLiteral("\\bvolatile\\b"), QStringLiteral("\\bbool\\b"),
        QStringLiteral("\\bif\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\bfor\\b"),
        QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bdo\\b"), QStringLiteral("\\breturn\\b"),
        QStringLiteral("\\bswitch\\b"), QStringLiteral("\\bcase\\b")
    };
    for (const QString &pattern: keywordPatterns)
    {
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    classFormat.setForeground(QColor(200, 70, 190));
    rule.pattern = QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b"));
    rule.format = classFormat;
    highlightingRules.append(rule);

    quolationFormat.setForeground(QColor(80, 150, 50));


    rule.pattern = QRegularExpression(QStringLiteral("([\"'])(.*?)\\1"));
    rule.format = quolationFormat;
    highlightingRules.append(rule);

    functionFormat.setForeground(QColor(10, 138, 240));

    rule.pattern = QRegularExpression(QStringLiteral("\\b(?:[A-Za-z0-9_]+(?=\\()|def\\s+[A-Za-z0-9_]+)\\b"));
    rule.format = functionFormat;
    highlightingRules.append(rule);

    preprocessorFormat.setForeground(QColor(64, 170, 255));
    preprocessorFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(QStringLiteral("(?:#include|import)\\b"));
    rule.format = preprocessorFormat;
    highlightingRules.append(rule);

    rule.pattern = QRegularExpression(QStringLiteral("\\bpackage\\b"));
    rule.format = preprocessorFormat;
    highlightingRules.append(rule);

    commentStartExpression = QRegularExpression(QStringLiteral("/\\*"));
    commentEndExpression = QRegularExpression(QStringLiteral("\\*/"));

    singleLineCommentFormat.setForeground(QColor(150, 150, 150));
    singleLineCommentFormat.setFontItalic(true);
    rule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    multiLineCommentFormat.setForeground(QColor(150, 150, 150));
    multiLineCommentFormat.setFontItalic(true);
}

bool CppHighlighter::isSupportedFileSuffix(const QString &fileName) const
{
    const QStringList supportedSuffixes = {
        "cpp", "cxx", "cc", "c", "h", "hpp", "hxx",
        "py", "go", "java"
    };
    QFileInfo fileInfo(fileName);
    QString suf = fileInfo.suffix().toLower();
    return supportedSuffixes.contains(suf);
}

void CppHighlighter::highlightBlock(const QString &text)
{
    QFileInfo fileInfo(currentFilePath);
    QString fileName = fileInfo.fileName();
    if (isSupportedFileSuffix(fileName)) {
        return;
    }

    for (const HighlightingRule &rule : std::as_const(highlightingRules))
    {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext())
        {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(commentStartExpression);
    while (startIndex >= 0)
    {
        QRegularExpressionMatch match = commentEndExpression.match(text, startIndex);
        int endIndex = match.capturedStart();
        int commentLength = 0;
        if (endIndex == -1)
        {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + match.capturedLength();
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = text.indexOf(commentStartExpression, startIndex + commentLength);
    }

}

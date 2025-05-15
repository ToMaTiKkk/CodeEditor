#include "cpphighlighter.h"
#include <utility> // добавил для использования std::as_const() из C++17
#include <QFileInfo>


CppHighlighter::CppHighlighter(QTextDocument *document, const QString &filePath, QObject *parent)
    : QSyntaxHighlighter(document),
    currentFilePath(filePath)
{
    HighlightingRule rule;

    //keywordFormat.setForeground(Qt::darkBlue);
    //keywordFormat.setForeground(QColor("#7F75DA"));
    //keywordFormat.setForeground(QColor(127, 127, 218));
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

    //classFormat.setForeground(Qt::darkMagenta);
    //classFormat.setForeground(QColor("#A6E22E"));
    //classFormat.setForeground(QColor(135, 206, 250));
    classFormat.setForeground(QColor(200, 70, 190));
    rule.pattern = QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b"));
    rule.format = classFormat;
    highlightingRules.append(rule);

    //singleLineCommentFormat.setForeground(Qt::gray);
    //singleLineCommentFormat.setForeground(QColor("#969696"));
    //singleLineCommentFormat.setForeground(QColor(255, 255, 255, 128));


    //multiLineCommentFormat.setForeground(Qt::gray);
    //multiLineCommentFormat.setForeground(QColor("#969696"));
    //multiLineCommentFormat.setForeground(QColor(255, 255, 255, 128));


    //quolationFormat.setForeground(Qt::darkGreen);
    //quolationFormat.setForeground(QColor("#E6DB74"));
    //quolationFormat.setForeground(QColor(245, 236, 64));

    //quolationFormat.setForeground(QColor(152, 229, 121));
    quolationFormat.setForeground(QColor(80, 150, 50));


    rule.pattern = QRegularExpression(QStringLiteral("\".*\""));
    rule.format = quolationFormat;
    highlightingRules.append(rule);

    //functionFormat.setFontItalic(true);
    //functionFormat.setForeground(Qt::blue);
    //functionFormat.setForeground(QColor("#A6E22E"));

    //functionFormat.setForeground(QColor(64, 170, 255));
    functionFormat.setForeground(QColor(10, 138, 240));

    //functionFormat.setForeground(QColor(255, 255, 255));
    rule.pattern = QRegularExpression(QStringLiteral("\\b(?:[A-Za-z0-9_]+(?=\\()|def\\s+[A-Za-z0-9_]+)\\b"));
    rule.format = functionFormat;
    highlightingRules.append(rule);

    //preprocessorFormat.setForeground(Qt::darkCyan);
    //preprocessorFormat.setForeground(QColor("#7F849C"));
    preprocessorFormat.setForeground(QColor(64, 170, 255));
    //preprocessorFormat.setForeground(QColor(127, 117, 218));
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
    // ---------------------------------------------
    // поменял строку с "for (const HighlightingRule &rule : qAsConst(highlightingRules))" чтобы убрать предупреждения при сборке, в связи с изменённым синтаксисом в Qt6
    // ---------------------------------------------
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

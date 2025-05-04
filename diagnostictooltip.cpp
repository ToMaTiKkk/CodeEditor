#include "diagnostictooltip.h"
#include <QDebug>
#include <QTextDocument>
#include <QStyleOption>

DiagnosticTooltip::DiagnosticTooltip(QWidget *parent) : QWidget(parent)
{
    // флаги для поведения всплывашки без рамки и активации
    setObjectName("DiagnosticTooltipInstance");
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground, true); // для отрисовки
    //setAttribute(Qt::WA_DeleteOnClose); // атвоматом удалять при скрытии

    // QLabel для текста
    m_label = new QLabel(this);
    m_label->setWordWrap(true); // для перенос по словам
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse); // можно мышкой выделять текст
    m_label->setStyleSheet("background: transparent; padding: 0; margin: 0;"); // просто через qss не подключалось

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5); // отступы внутри виджета
    layout->addWidget(m_label);
    setLayout(layout);


    // началььный размер будет подстраиваться под текст
    adjustSize();
}

void DiagnosticTooltip::setText(const QString &text)
{
    // текст может содержать html символы, заменяем переносы на <br>
    QString htmlText = Qt::mightBeRichText(text) ? text : text.toHtmlEscaped();
    htmlText.replace("\n", "<br>");
    m_label->setText(htmlText); // settext понимает простой html
    adjustSize(); // подгоняем под размер нового текста
}

// для использования MarkDown и сложных html
void DiagnosticTooltip::setRichText(const QString &richText)
{
    m_label->setTextFormat(Qt::RichText);
    m_label->setText(richText);
    adjustSize();
}
void DiagnosticTooltip::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this); //учитывая qss рисуем
}

#include "diagnostictooltip.h"
#include <QDebug>
#include <QTextDocument>
#include <QScreen>
#include <QGuiApplication>

DiagnosticTooltip::DiagnosticTooltip(QWidget *parent) : QWidget(parent)
{
    // флаги для поведения всплывашки без рамки и активации
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    //setAttribute(Qt::WA_DeleteOnClose); // атвоматом удалять при скрытии

    // QLabel для текста
    m_label = new QLabel(this);
    m_label->setWordWrap(true); // для перенос по словам
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse); // можно мышкой выделять текст

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5); // отступы внутри виджета
    layout->addWidget(m_label);
    setLayout(layout);

    // стилизацию ПЕРЕДЕЛАТЬ ОНА ЗАГЛУШКА
    setStyleSheet(R"(
        DiagnosticTooltip {
            background-color: #44475a;
            color: #f8f8f2;
            border: 1px solid #6272a4;
            border-radius: 4px;
            padding: 4px;
        }
        QLabel {
            background-color: transparent;
            border: none;
            padding: 0px;
            color: #f8f8f2;
        }
    )");

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

QPoint DiagnosticTooltip::calculateTooltipPosition(const QPoint& globalMousePos)
{
    if (!m_diagnosticTooltip) {
        return globalMousePos + QPoint(10, 15); // базовое смещение если виджета нет
    }

    QSize tipSize = m_diagnosticTooltip->sizeHint(); // берем рекомендуемый размер
    if (!tipSize.isValid()) {
        tipSize = m_diagnosticTooltip->size(); // или текущий, если реккомендованный невалидный
    }
    if (!tipSize.isValid() || tipSize.isEmpty()) {
        tipSize = QSize(200, 50); // запасной размер
        m_diagnosticTooltip->resize(tipSize);
    }

    QPoint pos = globalMousePos + QPoint(10, 15); // начальная позиция
    // получаем геометрию экрана где мышь
    QScreen *screen = QGuiApplication::screenAt(globalMousePos);
    if (!screen) {
        // экран не определили, поэтому базовую позицию используем
        qWarning() << "[CalcTooltipPos] Could not determine screen for position calculation.";
        return pos;
    }
    QRect screenGeometry = screen->availableGeometry(); // доступная геометрия без панели задач и тп

    // проверяем выход за правую границу
    if (pos.x() + tipSize.width() > screenGeometry.right()) {
        pos.setX(globalMousePos.x() - tipSize.width() - 10); // перемещаем влево от курсора
        // праверим левую границу
        if (pos.x() < screenGeometry.left()) {
            pos.setX(screenGeometry.left()); //прижимаем к левому краю
        }
    }

    // проверяем выход за нижнюю границу
    if (pos.y() + tipSize.height() > screenGeometry.bottom()) {
        pos.setY(globalMousePos.y() - tipSize.height() - 15); // перемещаем над курсором
        // праверим верхнюю границу
        if (pos.y() < screenGeometry.top()) {
            pos.setY(screenGeometry.top()); //прижимаем к верхнему краю
        }
    }

    // проверка на выход за левую или верхнюю грацниы если изначально сдвинули
    if (pos.x() < screenGeometry.left()) {
        pos.setX(screenGeometry.left());
    }
    if (pos.y() < screenGeometry.top()) {
        pos.setY(screenGeometry.top());
    }

    qDebug() << "[CalcTooltipPos] Mouse:" << globalMousePos << "TipSize:" << tipSize << "Screen:" << screenGeometry << "ResultPos:" << pos;
    return pos;
}

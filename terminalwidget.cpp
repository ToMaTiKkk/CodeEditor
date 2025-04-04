#include "terminalwidget.h"
#include <qtermwidget.h>
#include <QVBoxLayout>
#include <QFont>
#include <QKeySequence>
#include <QKeyEvent>
#include <QUrl>
#include <QDesktopServices>
#include <QApplication>
#include <QDebug>
#include <QColor>
#include <QPalette>
#include <QProcess>

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent)
{
    term_widget = new QTermWidget(this);
    if (!term_widget) {
        qCritical() << "Failed to create internal QTermWidget!";
        return;
    }
    term_widget->setObjectName("internalTermWidget");

    QFont termFont = QApplication::font();
    termFont.setFamily("Fira Code");
    termFont.setPointSize(12);
    termFont.setStyleHint(QFont::TypeWriter); //не убирпть!!!!!1
    term_widget->setTerminalFont(termFont);

    term_widget->setColorScheme(":/styles/Dark.colorscheme"); // по дефолту


    //term_widget->setBlinkingCursor(true); //как по мне так лучше
    //term_widget->setMargin(5);

    layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(term_widget);
    term_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);


    connect(term_widget, &QTermWidget::termKeyPressed, this, &TerminalWidget::handleKeyPress);
    connect(term_widget, &QTermWidget::urlActivated, this, &TerminalWidget::handleLinkActivation);

    qDebug() << "TerminalWidget wrapper created and configured.";
}

TerminalWidget::~TerminalWidget()
{
    qDebug() << "TerminalWidget wrapper destroyed.";
}

void TerminalWidget::setInputFocus()
{
    if (term_widget) {
        term_widget->setFocus();
    }
}

void TerminalWidget::handleKeyPress(QKeyEvent *event)
{
    if (!term_widget) return;

    if (event->matches(QKeySequence::Copy)) {
        term_widget->copyClipboard();
        event->accept();
    } else {
        event->ignore();
    }
}

void TerminalWidget::handleLinkActivation(const QUrl &url, bool fromContextMenu)
{
    Q_UNUSED(fromContextMenu);
    if (url.isValid()) {
        qDebug() << "Opening link:" << url.toString();
        QDesktopServices::openUrl(url);
    }
}
void TerminalWidget::applyColorScheme(const QString &schemePath)
{
    if (!term_widget) {
        return;
    }
    term_widget->setColorScheme(schemePath);
}
void TerminalWidget::sendCommand(const QString& command)
{
    if (!term_widget) {
        return;
    }
    QString commandToSend = command;
    if (!commandToSend.endsWith('\n')) {
        commandToSend.append('\n');
    }
    term_widget->sendText(commandToSend);
}

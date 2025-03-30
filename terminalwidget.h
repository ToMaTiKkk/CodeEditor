#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QWidget>

class QTermWidget;
class QVBoxLayout;
class QKeyEvent;
class QUrl;

class TerminalWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget();

    void setInputFocus();

private slots:
    void handleKeyPress(QKeyEvent *event);
    void handleLinkActivation(const QUrl &url, bool fromContextMenu);

private:
    QTermWidget *term_widget;
    QVBoxLayout *layout;
};

#endif

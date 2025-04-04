#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QWidget>
#include <QProcess>

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
    void applyColorScheme(const QString& schemePath);
    void sendCommand(const QString& command);

private slots:
    void handleKeyPress(QKeyEvent *event);
    void handleLinkActivation(const QUrl &url, bool fromContextMenu);

private:
    QTermWidget *term_widget;
    QVBoxLayout *layout;
};

#endif

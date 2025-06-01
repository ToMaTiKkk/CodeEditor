// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

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

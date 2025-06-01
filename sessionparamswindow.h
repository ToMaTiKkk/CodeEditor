// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONPARAMSWINDOW_H
#define SESSIONPARAMSWINDOW_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>

class SessionParamsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SessionParamsWindow(QWidget *parent = nullptr);
    QString getPassword() const;
    bool shouldSaveSession() const;
    int getSaveDays() const;

private slots:
    void onSaveCheckboxChanged(int state);

private:
    QLineEdit *passwordEdit;
    QCheckBox *saveCheckbox;
    QSpinBox *daysSpinBox;
    QLabel *daysLabel;
    QLabel *saveSessionLabel;
};

#endif // SESSIONPARAMSWINDOW_H

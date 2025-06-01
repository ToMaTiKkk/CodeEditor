// CodeEditor - A collaborative C++ IDE with LSP, chat, and terminal integration.
// Copyright (C) 2025 ToMaTiKkk
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lspsettingsdialog.h"
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

LspSettingsDialog::LspSettingsDialog(QWidget *parent, const QMap<QString, QString>& languages, const QMap<QString, QString>& paths)
    : QDialog(parent)
{
    setWindowTitle(tr("Настройки LSP-серверов"));
    m_form = new QFormLayout;
    
    // для каждой записи из languages
    for (auto it = languages.constBegin(); it != languages.constEnd(); ++it) {
        QString langId = it.key(); // "cpp", "python"
        QString display = it.value(); // "C/C++", "Python"
        QString initPath = paths.value(langId);
        
        // поле ввода пути
        QLineEdit *edit = new QLineEdit(initPath);
        edit->setPlaceholderText(tr("Не указан"));
        
        // поле кнопки обзора
        QPushButton *btn = new QPushButton(tr("..."));
        btn->setProperty("langId", langId); // сохраняем к какому языку относится
        connect(btn, &QPushButton::clicked, this, &LspSettingsDialog::onBrowse);
        
        // edit+btn в единый контейнер горизонтальный
        QWidget *hbox = new QWidget;
        QHBoxLayout *hl = new QHBoxLayout(hbox);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(edit);
        hl->addWidget(btn);
        
        // добавляем строку в форме метка:контейнер (edit+btn)
        m_form->addRow(display + ":", hbox);
        
        // сохраняем поле пути, чтобы потом его прочиатт
        m_lineEdits[langId] = edit;
    }
    
    // кнопки отмены и подтверждение
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    // общая компоновка, сверху форма, снизу - кнопки
    QVBoxLayout *mainL = new QVBoxLayout(this);
    mainL->addLayout(m_form);
    mainL->addWidget(bb);
}

void LspSettingsDialog::onBrowse()
{
    // находим кнопку отправителя
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    
    // определяем язык из свойста и находим соответствующие поле в адресом
    QString langId = btn->property("langId").toString();
    QLineEdit *edit = m_lineEdits.value(langId);
    if (!edit) return;
    
    // открывае диалог ввода, стартовая папка edit->text()
    QString sel = QFileDialog::getOpenFileName(this, tr("Выберите LSP-сервер для %1").arg(langId), edit->text(), tr("Исполняемый файл (*)"));
    if (!sel.isEmpty()) {
        edit->setText(sel);
    }    
}

QMap<QString, QString> LspSettingsDialog::selectedPaths() const 
{
    QMap<QString, QString> result;
    // перебираем все записи языков и их человеческий перевод, чтобы собрать итоговую информацию и отобразить
    for (auto it = m_lineEdits.constBegin(); it != m_lineEdits.constEnd(); ++it) {
        result[it.key()] = it.value()->text().trimmed();
    }
    return result;
}

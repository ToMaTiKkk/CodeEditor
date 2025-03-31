#include "sessionparamswindow.h"

SessionParamsWindow::SessionParamsWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Параметры сессии");
    setModal(true);
    setMinimumSize(600, 280);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout();

    // пароль
    passwordEdit = new QLineEdit(this);
    passwordEdit->setPlaceholderText("Введите пароль (мин. 4 символа)");
    passwordEdit->setEchoMode(QLineEdit::Password);
    // чекбокс для сохранения сессии
    saveCheckbox = new QCheckBox("Сохранить сессию", this);
    saveCheckbox->setChecked(false);

    daysLabel = new QLabel("На сколько дней:", this);
    daysLabel->hide();
    // спинбокс для дней (скрыт по умолчанию
    daysSpinBox = new QSpinBox(this);
    daysSpinBox->setRange(1, 7);
    daysSpinBox->setValue(1);
    daysSpinBox->setSuffix(" дней");
    daysSpinBox->hide();

    // соединение сигнала чекбокса
    connect(saveCheckbox, &QCheckBox::stateChanged,
            this, &SessionParamsWindow::onSaveCheckboxChanged);

    formLayout->addRow("Пароль сессии:", passwordEdit);
    formLayout->addRow(saveCheckbox);
    formLayout->addRow(daysLabel, daysSpinBox);

    QPushButton *okButton = new QPushButton("Создать", this);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);

    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(okButton, 0, Qt::AlignRight);
}

void SessionParamsWindow::onSaveCheckboxChanged(int state)
{
    bool checked = (state == Qt::Checked);
    daysLabel->setVisible(checked);
    daysSpinBox->setVisible(checked);
}

QString SessionParamsWindow::getPassword() const
{
    return passwordEdit->text();
}

bool SessionParamsWindow::shouldSaveSession() const
{
    return saveCheckbox->isChecked();
}

int SessionParamsWindow::getSaveDays() const
{
    return shouldSaveSession() ? daysSpinBox->value() : 0;
}

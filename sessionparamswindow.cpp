#include "sessionparamswindow.h"

SessionParamsWindow::SessionParamsWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Параметры сессии");
    setModal(true);
    setMinimumSize(390, 140);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setVerticalSpacing(20);

    // пароль
    passwordEdit = new QLineEdit(this);
    passwordEdit->setPlaceholderText("мин. 4 символа");
    passwordEdit->setEchoMode(QLineEdit::Password);

    // виджет контейнер для чекбокса и спинбокса
    QWidget *saveOptionsWidget = new QWidget(this);
    QHBoxLayout *saveOptionsLayout = new QHBoxLayout(saveOptionsWidget);
    saveOptionsLayout->setContentsMargins(0, 0, 0, 0);
    saveOptionsLayout->setSpacing(20);

    // чекбокс для сохранения сессии
    saveCheckbox = new QCheckBox(saveOptionsWidget);
    saveCheckbox->setChecked(false);
    saveCheckbox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // на
    daysLabel = new QLabel("на", saveOptionsWidget);
    daysLabel->hide();

    //лейбл сохранения сессии
    saveSessionLabel = new QLabel("Сохранить сессию", saveOptionsWidget);


    // спинбокс для дней (скрыт по умолчанию
    daysSpinBox = new QSpinBox(saveOptionsWidget);
    daysSpinBox->setRange(1, 7);
    daysSpinBox->setValue(1);
    daysSpinBox->setSuffix(" дней");
    daysSpinBox->hide();

    saveOptionsLayout->addWidget(saveSessionLabel);
    saveOptionsLayout->addWidget(daysLabel);
    saveOptionsLayout->addWidget(daysSpinBox);
    saveOptionsLayout->addStretch(1);
    saveOptionsLayout->addWidget(saveCheckbox);

    // соединение сигнала чекбокса
    connect(saveCheckbox, &QCheckBox::stateChanged,
            this, &SessionParamsWindow::onSaveCheckboxChanged);

    formLayout->addRow("Пароль сессии:", passwordEdit);
    formLayout->addRow(saveOptionsWidget);

    QPushButton *okButton = new QPushButton("Создать", this);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    bool ok;
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(okButton, 0, Qt::AlignRight);
    setLayout(mainLayout);
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

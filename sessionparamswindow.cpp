#include "sessionparamswindow.h"

SessionParamsWindow::SessionParamsWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Параметры сессии");
    setModal(true);
    setMinimumSize(400, 280);

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
    saveOptionsLayout->setSpacing(10);

    // виджет контейнер для спинбокса и текста перед ним
    QWidget *userWidget = new QWidget(this);
    QHBoxLayout *userLayout = new QHBoxLayout(userWidget);
    userLayout->setContentsMargins(0, 0, 0, 0);
    userLayout->setSpacing(10);

    // чекбокс для сохранения сессии
    saveCheckbox = new QCheckBox(saveOptionsWidget);
    saveCheckbox->setChecked(false);
    saveCheckbox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // на
    daysLabel = new QLabel("на", saveOptionsWidget);
    daysLabel->hide();

    //лейблы на оба виджета
    userAmountLabel=new QLabel("Количество пользователей:", userWidget);
    saveSessionLabel = new QLabel("Сохранить сессию", userWidget);


    // спинбокс для дней (скрыт по умолчанию
    daysSpinBox = new QSpinBox(saveOptionsWidget);
    daysSpinBox->setRange(1, 7);
    daysSpinBox->setValue(1);
    daysSpinBox->setSuffix(" дней");
    daysSpinBox->hide();

    //спинбокс для количества пользователей
    userAmountSpinBox = new QSpinBox();
    userAmountSpinBox->setRange(1, 5);
    userAmountSpinBox->setValue(2);

    saveOptionsLayout->addWidget(saveSessionLabel);
    saveOptionsLayout->addWidget(daysLabel);
    saveOptionsLayout->addWidget(daysSpinBox);
    saveOptionsLayout->addWidget(saveCheckbox);
    saveOptionsLayout->addStretch();

    userLayout->addWidget(userAmountLabel);
    userLayout->addWidget(userAmountSpinBox);

    // соединение сигнала чекбокса
    connect(saveCheckbox, &QCheckBox::stateChanged,
            this, &SessionParamsWindow::onSaveCheckboxChanged);

    formLayout->addRow("Пароль сессии:", passwordEdit);
    formLayout->addRow(saveOptionsWidget);
    formLayout->addRow(userWidget);

    QPushButton *okButton = new QPushButton("Создать", this);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);

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

int SessionParamsWindow::getUsers() const
{
    return userAmountSpinBox->value();
}

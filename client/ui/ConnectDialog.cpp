#include "ConnectDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ConnectDialog::ConnectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("新建连接");
    resize(380, 230);

    m_connectionNameEdit = new QLineEdit(this);
    m_connectionNameEdit->setText("本地连接");

    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setText("127.0.0.1");

    m_portSpinBox = new QSpinBox(this);
    m_portSpinBox->setRange(1, 65535);
    m_portSpinBox->setValue(3306);

    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setText("root");

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setText("123456");

    auto *formLayout = new QFormLayout;
    formLayout->addRow("连接名：", m_connectionNameEdit);
    formLayout->addRow("主机地址：", m_hostEdit);
    formLayout->addRow("端口：", m_portSpinBox);
    formLayout->addRow("用户名：", m_usernameEdit);
    formLayout->addRow("密码：", m_passwordEdit);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this
    );

    buttonBox->button(QDialogButtonBox::Ok)->setText("连接");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

QString ConnectDialog::connectionName() const
{
    return m_connectionNameEdit->text().trimmed();
}

QString ConnectDialog::host() const
{
    return m_hostEdit->text().trimmed();
}

int ConnectDialog::port() const
{
    return m_portSpinBox->value();
}

QString ConnectDialog::username() const
{
    return m_usernameEdit->text().trimmed();
}

QString ConnectDialog::password() const
{
    return m_passwordEdit->text();
}
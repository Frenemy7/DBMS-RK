#ifndef CONNECTDIALOG_H
#define CONNECTDIALOG_H

#include <QDialog>

class QLineEdit;
class QSpinBox;

class ConnectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectDialog(QWidget *parent = nullptr);

    QString connectionName() const;
    QString host() const;
    int port() const;
    QString username() const;
    QString password() const;

private:
    QLineEdit *m_connectionNameEdit;
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpinBox;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
};

#endif
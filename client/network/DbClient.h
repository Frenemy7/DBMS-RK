#ifndef DB_CLIENT_H
#define DB_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QStringList>
#include <QVector>

class DbClient : public QObject {
    Q_OBJECT

public:
    struct QueryResponse {
        bool success = false;
        QString database;
        QString message;
        QStringList headers;
        QVector<QStringList> rows;
    };

    explicit DbClient(QObject *parent = nullptr);

    void connectToServer(const QString& host, quint16 port, const QString& username, const QString& password);
    void executeSql(const QString& sql);
    bool isConnected() const;
    QString username() const;

signals:
    void connected();
    void loginSucceeded();
    void disconnected();
    void responseReceived(const DbClient::QueryResponse& response);
    void errorReceived(const QString& message);

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    enum class PacketType : quint16 {
        Login = 1,
        ExecuteSql = 2,
        Response = 3,
        Error = 4
    };

    QTcpSocket* socket_;
    QByteArray buffer_;
    QString username_;
    QString password_;
    bool authenticated_ = false;

    void sendPacket(PacketType type, const QByteArray& body);
    void processBuffer();
    void handlePacket(PacketType type, const QByteArray& body);
    QueryResponse parseResponse(const QString& payload) const;
    QString unescapeCell(const QString& value) const;
};

Q_DECLARE_METATYPE(DbClient::QueryResponse)

#endif // DB_CLIENT_H

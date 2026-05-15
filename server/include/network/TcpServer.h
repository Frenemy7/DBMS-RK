#ifndef RUANKO_TCP_SERVER_H
#define RUANKO_TCP_SERVER_H

#include "NetworkProtocol.h"
#include "SqlService.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QObject>

namespace Network {

    class TcpServer : public QTcpServer {
    public:
        explicit TcpServer(SqlService* service, QObject* parent = nullptr);

    protected:
        void incomingConnection(qintptr socketDescriptor) override;

    private:
        struct ClientState {
            QByteArray buffer;
            QString username;
            bool authenticated = false;
        };

        SqlService* service_;
        QHash<QTcpSocket*, ClientState> clients_;

        void onReadyRead(QTcpSocket* socket);
        void onDisconnected(QTcpSocket* socket);
        void processBuffer(QTcpSocket* socket);
        void handlePacket(QTcpSocket* socket, PacketType type, const QByteArray& body);
        void sendPacket(QTcpSocket* socket, PacketType type, const QByteArray& body);
    };

}

#endif // RUANKO_TCP_SERVER_H

#include "../../include/network/TcpServer.h"
#include "../../include/network/NetworkProtocol.h"

#include <QByteArray>
#include <QDataStream>
#include <QDebug>

namespace Network {

    TcpServer::TcpServer(SqlService* service, QObject* parent)
        : QTcpServer(parent), service_(service) {}

    void TcpServer::incomingConnection(qintptr socketDescriptor) {
        auto* socket = new QTcpSocket(this);
        if (!socket->setSocketDescriptor(socketDescriptor)) {
            socket->deleteLater();
            return;
        }

        clients_.insert(socket, ClientState{});

        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() { onDisconnected(socket); });
    }

    void TcpServer::onReadyRead(QTcpSocket* socket) {
        clients_[socket].buffer.append(socket->readAll());
        processBuffer(socket);
    }

    void TcpServer::onDisconnected(QTcpSocket* socket) {
        clients_.remove(socket);
        socket->deleteLater();
    }

    void TcpServer::processBuffer(QTcpSocket* socket) {
        auto& state = clients_[socket];

        while (state.buffer.size() >= static_cast<int>(sizeof(std::uint32_t) + sizeof(std::uint16_t))) {
            QDataStream header(state.buffer.left(6));
            header.setByteOrder(QDataStream::BigEndian);

            quint32 bodyLength = 0;
            quint16 packetType = 0;
            header >> bodyLength >> packetType;

            if (bodyLength > kMaxPacketSize) {
                sendPacket(socket, PacketType::Error, "packet too large");
                socket->disconnectFromHost();
                return;
            }

            const int packetSize = 6 + static_cast<int>(bodyLength);
            if (state.buffer.size() < packetSize) return;

            const QByteArray body = state.buffer.mid(6, bodyLength);
            state.buffer.remove(0, packetSize);
            handlePacket(socket, static_cast<PacketType>(packetType), body);
        }
    }

    void TcpServer::handlePacket(QTcpSocket* socket, PacketType type, const QByteArray& body) {
        auto& state = clients_[socket];

        if (type == PacketType::Login) {
            const QString payload = QString::fromUtf8(body);
            const QStringList parts = payload.split('\n');
            const QString username = parts.value(0);
            const QString password = parts.value(1);

            if (service_ && service_->login(username.toStdString(), password.toStdString())) {
                state.username = username;
                state.authenticated = true;
                sendPacket(socket, PacketType::Response, "STATUS\tOK\nMESSAGE_BEGIN\nLogin OK.\nMESSAGE_END\n");
            } else {
                sendPacket(socket, PacketType::Error, "Login failed.");
            }
            return;
        }

        if (!state.authenticated) {
            sendPacket(socket, PacketType::Error, "Please login first.");
            return;
        }

        if (type == PacketType::ExecuteSql) {
            const QString sql = QString::fromUtf8(body);
            const SqlResult result = service_->execute(sql.toStdString(), state.username.toStdString());
            sendPacket(socket, result.success ? PacketType::Response : PacketType::Error, QByteArray::fromStdString(result.text));
            return;
        }

        sendPacket(socket, PacketType::Error, "Unknown packet type.");
    }

    void TcpServer::sendPacket(QTcpSocket* socket, PacketType type, const QByteArray& body) {
        QByteArray packet;
        QDataStream stream(&packet, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << static_cast<quint32>(body.size());
        stream << static_cast<quint16>(type);
        packet.append(body);
        socket->write(packet);
    }

}

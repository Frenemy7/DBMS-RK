#include "DbClient.h"

#include <QDataStream>

namespace {
    constexpr quint32 MaxPacketSize = 16 * 1024 * 1024;
}

DbClient::DbClient(QObject *parent)
    : QObject(parent),
      socket_(new QTcpSocket(this))
{
    qRegisterMetaType<DbClient::QueryResponse>("DbClient::QueryResponse");

    connect(socket_, &QTcpSocket::connected, this, &DbClient::onConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &DbClient::onDisconnected);
    connect(socket_, &QTcpSocket::readyRead, this, &DbClient::onReadyRead);
    connect(socket_, &QTcpSocket::errorOccurred, this, &DbClient::onSocketError);
}

void DbClient::connectToServer(const QString& host, quint16 port, const QString& username, const QString& password)
{
    username_ = username;
    password_ = password;
    authenticated_ = false;
    buffer_.clear();

    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->abort();
    }

    socket_->connectToHost(host, port);
}

void DbClient::executeSql(const QString& sql)
{
    if (!isConnected()) {
        emit errorReceived("尚未连接到服务端。");
        return;
    }

    sendPacket(PacketType::ExecuteSql, sql.toUtf8());
}

bool DbClient::isConnected() const
{
    return socket_->state() == QAbstractSocket::ConnectedState && authenticated_;
}

QString DbClient::username() const
{
    return username_;
}

void DbClient::onReadyRead()
{
    buffer_.append(socket_->readAll());
    processBuffer();
}

void DbClient::onConnected()
{
    emit connected();
    sendPacket(PacketType::Login, QString("%1\n%2").arg(username_, password_).toUtf8());
}

void DbClient::onDisconnected()
{
    authenticated_ = false;
    emit disconnected();
}

void DbClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    emit errorReceived(socket_->errorString());
}

void DbClient::sendPacket(PacketType type, const QByteArray& body)
{
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint32>(body.size());
    stream << static_cast<quint16>(type);
    packet.append(body);
    socket_->write(packet);
}

void DbClient::processBuffer()
{
    while (buffer_.size() >= 6) {
        QDataStream header(buffer_.left(6));
        header.setByteOrder(QDataStream::BigEndian);

        quint32 bodyLength = 0;
        quint16 packetType = 0;
        header >> bodyLength >> packetType;

        if (bodyLength > MaxPacketSize) {
            emit errorReceived("服务端返回包过大。");
            socket_->disconnectFromHost();
            return;
        }

        const int packetSize = 6 + static_cast<int>(bodyLength);
        if (buffer_.size() < packetSize) return;

        const QByteArray body = buffer_.mid(6, bodyLength);
        buffer_.remove(0, packetSize);
        handlePacket(static_cast<PacketType>(packetType), body);
    }
}

void DbClient::handlePacket(PacketType type, const QByteArray& body)
{
    const QString payload = QString::fromUtf8(body);

    if (type == PacketType::Response) {
        QueryResponse response = parseResponse(payload);
        if (payload.contains("Login OK.")) {
            authenticated_ = true;
            emit loginSucceeded();
        } else {
            emit responseReceived(response);
        }
        return;
    }

    if (type == PacketType::Error) {
        QueryResponse response = parseResponse(payload);
        if (!response.message.isEmpty() || !response.headers.isEmpty()) {
            emit responseReceived(response);
        } else {
            emit errorReceived(payload);
        }
        return;
    }

    emit errorReceived("收到未知响应。");
}

DbClient::QueryResponse DbClient::parseResponse(const QString& payload) const
{
    QueryResponse response;
    const QStringList lines = payload.split('\n');
    bool inMessage = false;
    bool inTable = false;

    for (const QString& rawLine : lines) {
        const QString line = rawLine;

        if (line == "MESSAGE_BEGIN") {
            inMessage = true;
            continue;
        }
        if (line == "MESSAGE_END") {
            inMessage = false;
            continue;
        }
        if (line == "RESULT_TABLE_BEGIN") {
            inTable = true;
            continue;
        }
        if (line == "RESULT_TABLE_END") {
            inTable = false;
            continue;
        }

        if (inMessage) {
            response.message += line + '\n';
            continue;
        }

        const QStringList parts = line.split('\t');
        if (parts.isEmpty()) continue;

        if (parts.first() == "STATUS" && parts.size() > 1) {
            response.success = parts.at(1) == "OK";
        } else if (parts.first() == "DATABASE" && parts.size() > 1) {
            response.database = unescapeCell(parts.at(1));
        } else if (inTable && parts.first() == "HEADERS") {
            for (int i = 1; i < parts.size(); ++i) {
                response.headers << unescapeCell(parts.at(i));
            }
        } else if (inTable && parts.first() == "ROW") {
            QStringList row;
            for (int i = 1; i < parts.size(); ++i) {
                row << unescapeCell(parts.at(i));
            }
            response.rows.append(row);
        }
    }

    response.message = response.message.trimmed();
    if (response.message.isEmpty()) {
        response.message = payload.trimmed();
    }
    return response;
}

QString DbClient::unescapeCell(const QString& value) const
{
    QString result;
    bool escaping = false;
    for (QChar ch : value) {
        if (escaping) {
            if (ch == 't') result += '\t';
            else if (ch == 'n') result += '\n';
            else if (ch == 'r') result += '\r';
            else result += ch;
            escaping = false;
        } else if (ch == '\\') {
            escaping = true;
        } else {
            result += ch;
        }
    }
    if (escaping) result += '\\';
    return result;
}

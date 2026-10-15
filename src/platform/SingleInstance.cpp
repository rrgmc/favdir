#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>

namespace {
constexpr int TimeoutMs = 1000;
constexpr char ShowCommand[] = "show\n";
} // namespace

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
    // Per user: the home path is unique per account on both platforms.
    const QByteArray hash = QCryptographicHash::hash(QDir::homePath().toUtf8(),
                                                     QCryptographicHash::Sha1)
                                .toHex()
                                .left(12);
    m_name = QStringLiteral("favdir-") + QString::fromLatin1(hash);
}

bool SingleInstance::start()
{
    {
        QLocalSocket socket;
        socket.connectToServer(m_name);
        if (socket.waitForConnected(TimeoutMs)) {
            socket.write(ShowCommand);
            socket.waitForBytesWritten(TimeoutMs);
            socket.disconnectFromServer();
            return false;
        }
    }

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_name)) {
        // A crashed instance can leave a stale socket file behind on macOS.
        QLocalServer::removeServer(m_name);
        m_server->listen(m_name);
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                if (socket->readAll().contains("show"))
                    emit showRequested();
                socket->disconnectFromServer();
            });
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    return true;
}

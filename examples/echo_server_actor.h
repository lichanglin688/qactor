#ifndef ECHO_SERVER_ACTOR_H
#define ECHO_SERVER_ACTOR_H

#include <actor.h>

#include <QDebug>
#include <QFuture>
#include <QHostAddress>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

class EchoServerActor final : public Actor
{
public:
    explicit EchoServerActor(ExecThread *thread)
        : Actor(thread)
    {
    }

    QFuture<quint16> start()
    {
        return async::postWithResult(execThread(), [this] {
            connect(&m_server, &QTcpServer::newConnection, &m_server,
                    [this] { acceptConnections(); });
            m_server.listen(QHostAddress::LocalHost, 0);
            return m_server.serverPort();
        });
    }

private:
    void acceptConnections()
    {
        while (QTcpSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, socket,
                    [socket] { socket->write(socket->readAll()); });
            connect(socket, &QTcpSocket::disconnected, socket,
                    &QObject::deleteLater);
        }
    }

    void onShutdown() override
    {
        m_server.close();
        qInfo() << "TCP echo server stopped on"
                << QThread::currentThread()->objectName();
    }

    QTcpServer m_server;
};

#endif

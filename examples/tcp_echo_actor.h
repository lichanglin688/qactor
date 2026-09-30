#ifndef TCP_ECHO_ACTOR_H
#define TCP_ECHO_ACTOR_H

#include <actor.h>

#include <QByteArray>
#include <QDebug>
#include <QFuture>
#include <QHostAddress>
#include <QTcpSocket>
#include <QThread>

class TcpEchoActor final : public Actor
{
public:
    explicit TcpEchoActor(ExecThread *thread)
        : Actor(thread)
    {
        m_socket = new QTcpSocket();
    }

    void connectToHost(quint16 port)
    {
        async::post(execThread(), [this, port] {
            m_socket->connectToHost(QHostAddress::LocalHost, port);
            m_socket->waitForConnected(5000);
        });
    }

    // 阻塞等待只发生在 actor 自己的线程上，主线程事件循环不受影响。
    QFuture<QByteArray> echo(QByteArray request)
    {
        return async::postWithResult(execThread(), [this, request = std::move(request)] {
            m_socket->write(request);
            QByteArray response;
            while (response.size() < request.size() && m_socket->waitForReadyRead(5000))
                response += m_socket->readAll();
            return response;
        });
    }

private:
    void onShutdown() override
    {
        delete m_socket;
        m_socket = nullptr;
        qInfo() << "TCP socket closed on"
                << QThread::currentThread()->objectName();
    }

    QTcpSocket *m_socket = nullptr;
};

#endif

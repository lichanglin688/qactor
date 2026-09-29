#include "echo_server_actor.h"
#include "tcp_echo_actor.h"

#include <QCoreApplication>
#include <QFuture>

#include <cstdlib>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    ExecThread serverThread("echoServer");
    EchoServerActor *server = serverThread.spawn<EchoServerActor>().result();
    const quint16 port = server->start().result();

    ExecThread clientThread("tcpClient");
    TcpEchoActor *client = clientThread.spawn<TcpEchoActor>().result();
    const QByteArray request = QByteArrayLiteral("hello over TCP");

    client->connectToHost(port);

    client->echo(request).then(&app, [&app, request](QFuture<QByteArray> future) {
        const QByteArray response = future.result();
        qInfo() << "Sent:" << request;
        qInfo() << "Received:" << response;
        app.exit(response == request ? EXIT_SUCCESS : EXIT_FAILURE);
    });

    const int exitCode = app.exec();

    client->shutdown();
    client->deleteLater();
    clientThread.stop();

    server->shutdown();
    server->deleteLater();
    serverThread.stop();

    return exitCode;
}

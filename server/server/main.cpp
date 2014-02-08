#include <stdio.h>

#include <QDebug>
#include <QString>
#include <QRegExp>
#include <QMap>

#include <SFML/System.hpp>
#include <SFML/Network.hpp>

QMap<QString, sf::TcpSocket*> players;

QString receiveData(sf::TcpSocket *socket)
{
    char in[1000];
    std::size_t received;

    if (socket->receive(in, sizeof(in), received) != sf::Socket::Done)
        return QString();

    return QString(in);
}

sf::Socket::Status sendMessage(sf::TcpSocket *socket, QString message)
{
    return socket->send(message.toUtf8().data(), sizeof(message));
}

void clientHandler(sf::TcpSocket* socket)
{
    sf::String address = socket->getRemoteAddress().toString();

    QString introductionMessage = receiveData(socket);

    QRegExp introductionRe("<(.+)>");

    if (introductionRe.indexIn(introductionMessage) < 0)
    {
        qDebug() << QString("`%1` is a bad introduction").arg(introductionMessage);
        return;
    }

    QString playerName = introductionRe.cap(1);

    // check if this socket is handling already
    // if (clients.find(address) != clients.end())
    //    return;

    // clients[address] = socket;
    players[playerName] = socket;

    qDebug() << QString("Client connected: %1\n").arg(address);

    while (true)
    {
        if (socket->send(out, sizeof(out)) != sf::Socket::Done)
            continue;

        // std::cout << "Message sent to the client: \"" << out << "\"" << std::endl;

        // Receive a message back from the client
        char in[128];
        std::size_t received;

        if (socket->receive(in, sizeof(in), received) != sf::Socket::Done)
        {
            return;
        }

        printf("Answer received from the client: `%s`\n", in);
    }
}

int main()
{
    unsigned short port = 9501;

    sf::TcpListener listener;

    if (listener.listen(port) != sf::Socket::Done)
        return 0;

    qDebug() << QString("Server is listening to port %1, waiting for connections...").arg(port);

    while (true)
    {
        sf::TcpSocket socket;

        if (listener.accept(socket) != sf::Socket::Done)
            continue;

        sf::Thread thread(&clientHandler, &socket);

        thread.launch();
    }

    return 0;
}


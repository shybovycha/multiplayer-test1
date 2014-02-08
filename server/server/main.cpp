#include <stdio.h>

#include <QDebug>
#include <QString>
#include <QRegExp>
#include <QMap>

#include <SFML/System.hpp>
#include <SFML/Network.hpp>

class Player
{
public:
    Player(sf::TcpSocket *socket) : socket(socket), position(sf::Vector2f(0, 0))
    {
    }

    Player(sf::TcpSocket *socket, const sf::Vector2f &position) : socket(socket), position(position)
    {
    }

    sf::Vector2f getPosition()
    {
        return position;
    }

    sf::TcpSocket* getSocket()
    {
        return socket;
    }

    void setPosition(const sf::Vector2f &position)
    {
        this->position = position;
    }

protected:
    sf::TcpSocket* socket;
    sf::Vector2f position;
};

QMap<QString, Player*> players;

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
    return socket->send(message.toUtf8().data(), message.toUtf8().size());
}

QString parseIntroductionMessage(const QString &message)
{
    QRegExp introductionRe("<(.+)>");

    if (introductionRe.indexIn(message) < 0)
    {
        qDebug() << QString("`%1` is a bad introduction").arg(message);
        return QString("");
    }

    QString playerName = introductionRe.cap(1);

    // check if this socket is handling already
    if (players.find(playerName) != players.end())
    {
        qDebug() << QString("Name `%1` has been already taken").arg(playerName);
        return QString("");
    }

    return playerName;
}

bool sendPlayersState(sf::TcpSocket* socket)
{
    QString msg = QString("%1.").arg(players.size());

    for (int i = 0; i < players.size(); i++)
    {
        QString name = players.keys().at(i);
        Player *player = players.values().at(i);
        msg += QString("[%1];%2;%3.").arg(name).arg(player->getPosition().x).arg(player->getPosition().y);
    }

    return (sendMessage(socket, msg) == sf::Socket::Done);
}

void broadcastMessage(const QString &message, const QString &senderName)
{
    for (int i = 0; i < players.size(); i++)
    {
        if (players.keys().at(i) == senderName)
            continue;

        sendMessage(players.values().at(i)->getSocket(), message);
    }
}

bool parseUpdateMessage(const QString &message)
{
    QRegExp playerRe("\\[([^]]+)\\];(\\d+\\.?\\d*);(\\d+\\.?\\d*).");

    if (playerRe.indexIn(message) < 0)
    {
        qDebug() << QString("`%1` is not a valid update message").arg(message);
        return false;
    }

    QString playerName = playerRe.cap(1);
    float x = playerRe.cap(2).toFloat(), y = playerRe.cap(3).toFloat();

    if (players.find(playerName) == players.end())
    {
        qDebug() << QString("Received message from ghost! %1 says `%2`").arg(playerName).arg(message);
        return false;
    }

    players[playerName]->setPosition(sf::Vector2f(x, y));

    broadcastMessage(message, playerName);

    return true;
}

void clientHandler(sf::TcpSocket* socket)
{
    QString address = socket->getRemoteAddress().toString().c_str();
    QString introductionMessage = receiveData(socket);
    QString playerName = parseIntroductionMessage(introductionMessage);

    if (playerName.isEmpty())
        return;

    Player *player = new Player(socket);
    players[playerName] = player;

    if (!sendPlayersState(socket))
    {
        qDebug() << QString("Could not contact client. Shutting him down");
        return;
    }

    qDebug() << QString("Client connected: %1 (%2)").arg(playerName).arg(address);

    while (true)
    {
        QString message = receiveData(socket);

        if (message.isEmpty())
        {
            qDebug() << QString("%1 has left").arg(playerName);
            return;
        }

        if (!parseUpdateMessage(message))
        {
            qDebug() << QString("Received an invalid message from %1 (message: `%2`).").arg(playerName).arg(message);
            continue;
        }
    }
}

int main()
{
    unsigned short port = 19501;

    sf::TcpListener listener;

    if (listener.listen(port) != sf::Socket::Done)
        return 0;

    listener.setBlocking(false);

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


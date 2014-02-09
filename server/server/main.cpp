#include <stdio.h>

#include <QDebug>
#include <QString>
#include <QRegExp>
#include <QMap>

#include <SFML/System.hpp>
#include <SFML/Network.hpp>

#define PING_TIMEOUT 10

struct Player
{
    sf::IpAddress ip;
    unsigned short port;
    sf::Clock updateTimer;
};

class ServerApplication
{
public:
    ServerApplication(unsigned short serverPort)
    {
        this->serverPort = serverPort;
        this->socket = new sf::UdpSocket();
        this->socket->bind(serverPort);
        this->socket->setBlocking(false);
    }

    QString findPlayerByPeer(const sf::IpAddress &address, unsigned short port)
    {
        for (int i = 0; i < players.size(); i++)
        {
            Player *player = players.values().at(i);

            if ((player->ip.toInteger() == address.toInteger()) && (player->port == port))
                return players.keys().at(i);
        }

        return QString("");
    }

    void broadcastMessage(const std::string &sender, sf::Packet &packet)
    {
        for (int i = 0; i < players.size(); i++)
        {
            Player *player = players.values().at(i);

            if (players.keys().at(i).toStdString() == sender)
                continue;

            socket->send(packet, player->ip, player->port);
        }
    }

    void sendDieMessage(const std::string &name)
    {
        sf::Packet packet;
        packet << name << "die";
        broadcastMessage(name, packet);
    }

    void sendAtMessage(const std::string &name, float x, float y)
    {
        sf::Packet packet;
        packet << name << "at" << x << y;
        broadcastMessage(name, packet);
    }

    void updateSleepingPeers()
    {
        sf::Packet packet;

        packet << "server" << "ping";

        for (int i = 0; i < players.size(); i++)
        {
            Player *player = players.values().at(i);

            if (player->updateTimer.getElapsedTime().asSeconds() > PING_TIMEOUT)
            {
                socket->send(packet, player->ip, player->port);
            }
        }
    }

    void mainFunc()
    {
        while (true)
        {
            sf::IpAddress senderIp;
            unsigned short senderPort;
            sf::Packet packet;

            sf::Socket::Status status = socket->receive(packet, senderIp, senderPort);

            QString existingName = findPlayerByPeer(senderIp, senderPort);

            if (status != sf::Socket::Done)
            {
                /*qDebug() << QString("%1:%2 died").arg(senderIp.toString().c_str()).arg(senderPort);

                if (existingName.isEmpty())
                    continue;

                players.remove(existingName);

                sendDieMessage(existingName.toStdString());*/

                continue;
            }

            std::string senderName, command;

            packet >> senderName >> command;

            if (existingName.isEmpty())
            {
                Player *player = new Player();
                player->ip = senderIp;
                player->port = senderPort;

                existingName = QString(senderName.c_str());

                players[existingName] = player;

                qDebug() << QString("%1 joined").arg(existingName);
            }

            qDebug() << QString("%1:%2 says: %3.%4").arg(senderIp.toString().c_str()).arg(senderPort).arg(senderName.c_str()).arg(command.c_str());

            if (command == "at")
            {
                float x, y;

                packet >> x >> y;

                sendAtMessage(senderName, x, y);
            }

//            sf::Thread updatePeersThread(&ServerApplication::updateSleepingPeers, this);
//            updatePeersThread.launch();
        }
    }

protected:
    QMap<QString, Player*> players;
    unsigned short serverPort;
    sf::UdpSocket *socket;
};

int main()
{
    unsigned short port = 19501;

    ServerApplication app(port);

    app.mainFunc();

    return 0;
}


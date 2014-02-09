#include <QString>
#include <QStringList>
#include <QMap>
#include <QRegExp>
#include <QDebug>

#include <stdio.h>

#include <vector>
#include <map>

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Network.hpp>
#include <SFML/Window.hpp>

#define PLAYER_UPDATE_DELAY 0.02

class Player
{
public:
    Player() : position(sf::Vector2f(0, 0))
    {
        previousPosition = position;
        lastUpdateTime = 1.0;
    }

    Player(const sf::Vector2f &pos) : position(pos)
    {
        previousPosition = position;
        lastUpdateTime = 1.0;
    }

    void update()
    {
        if (lastUpdateTime == 0.0)
            lastUpdateTime = 1.0;

        double t = updateTimer.getElapsedTime().asMilliseconds() / lastUpdateTime;

        if (t > 1.0)
            t = 1.0;

        // position = lerp(previousPosition, position, t);
    }

    void setPosition(const sf::Vector2f &pos)
    {
        this->previousPosition = this->position;
        this->position = pos;
        this->lastUpdateTime = this->updateTimer.getElapsedTime().asMilliseconds();
        this->updateTimer.restart();
    }

    sf::Vector2f getPosition()
    {
        return this->position;
    }

protected:
    sf::Vector2f position;
    sf::Vector2f previousPosition;
    sf::Clock updateTimer;
    double lastUpdateTime;

    sf::Vector2f lerp(const sf::Vector2f &start, const sf::Vector2f &end, float t)
    {
        return start + ((end - start) * t);
    }
};

class ClientApplication
{
public:
    ClientApplication(const QString &playerName, const QString &serverIp, const unsigned short serverPort)
    {
        this->serverIp = serverIp.toStdString().c_str();
        this->serverPort = serverPort;
        this->playerName = playerName;
        this->players.clear();
        this->players[playerName] = new Player();
        this->socket = new sf::UdpSocket();
        this->socket->setBlocking(false);
        this->font = new sf::Font();
        this->font->loadFromFile("arial.ttf");
        this->isWindowActive = true;
    }

    void sendingFunc()
    {
        sf::Clock delayTimer;

        while (true)
        {
            receivingFunc();

            if (delayTimer.getElapsedTime().asSeconds() > PLAYER_UPDATE_DELAY)
            {
                sendAtMessage();
                delayTimer.restart();
            }
        }
    }

    void receivingFunc()
    {
            sf::Packet packet;

            sf::IpAddress senderIp;
            unsigned short senderPort;
            sf::Socket::Status status = this->socket->receive(packet, senderIp, senderPort);

            if (status != sf::Socket::Done)
            {
                // qDebug() << "Error";
                return;
            }

            std::string name, command;

            packet >> name >> command;

            if (name.empty())
                return;

            qDebug() << QString("[%1] says `%2`").arg(name.c_str()).arg(command.c_str());

            QString packetName = name.c_str();

            if (players.find(packetName) == players.end())
            {
                players[packetName] = new Player();
            }

            if (command == "at")
            {
                float x, y;

                packet >> x >> y;

                players[packetName]->setPosition(sf::Vector2f(x, y));
            } else if (command == "die")
            {
                players.remove(packetName);
            } else if (command == "ping")
            {
                sendAtMessage();
            }
    }

    void sendAtMessage()
    {
        sf::Packet packet;

        packet << this->playerName.toStdString() << "at" << this->players[this->playerName]->getPosition().x << this->players[this->playerName]->getPosition().y;

        this->socket->send(packet, this->serverIp, this->serverPort);
    }

    void mainFunc()
    {
        sendAtMessage();

        sf::Thread receivingThread(&ClientApplication::receivingFunc, this);
        receivingThread.launch();

        sf::Thread sendingThread(&ClientApplication::sendingFunc, this);
        sendingThread.launch();

        window = new sf::RenderWindow(sf::VideoMode(800, 600), "Multiplayer Client");

        while (window->isOpen())
        {
            // check all the window's events that were triggered since the last iteration of the loop
            sf::Event event;

            while (window->pollEvent(event))
            {
                // "close requested" event: we close the window
                if (event.type == sf::Event::Closed)
                    window->close();

                if (event.type == sf::Event::LostFocus)
                    isWindowActive = false;

                if (event.type == sf::Event::GainedFocus)
                    isWindowActive = true;
            }

            handleUserInput();
            render();
        }
    }

    void render()
    {
        window->clear(sf::Color::Black);

        for (int i = 0; i < players.size(); i++)
        {
            Player *player = players.values().at(i);

            if (players.keys().at(i) != this->playerName)
                player->update();

            sf::CircleShape shape(10.0);

            shape.setFillColor(sf::Color(rand() % 255, rand() % 255, rand() % 255));
            shape.setOutlineThickness(4.0);
            shape.setOutlineColor(sf::Color(50, 200, 25));
            shape.setPosition(player->getPosition());

            window->draw(shape);

            sf::Text text(players.keys().at(i).toStdString(), *this->font);

            text.setCharacterSize(10);
            text.setPosition(player->getPosition());

            window->draw(text);
        }

        window->display();
    }

    void handleUserInput()
    {
        if (!isWindowActive)
            return;

        sf::Vector2f pos = this->players[this->playerName]->getPosition();

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
            pos += sf::Vector2f(0, -1);

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
            pos += sf::Vector2f(0, 1);

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
            pos += sf::Vector2f(-1, 0);

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
            pos += sf::Vector2f(1, 0);

        this->players[this->playerName]->setPosition(pos);
    }

protected:
    sf::IpAddress serverIp;
    unsigned short serverPort;
    sf::UdpSocket *socket;

    QString playerName;
    QMap<QString, Player*> players;
    sf::RenderWindow *window;
    sf::Font *font;
    bool isWindowActive;
};

int main(int argc, char **argv)
{
    QString name;

    if (argc < 2)
    {
        printf("Too few arguments. Usage:\n\t%s [name]\n", argv[0]);
        //return 1;
        name = QString("player-%1").arg(rand() % 1500);
    } else
    {
        name = QString("player-%1").arg(argv[1]);
    }

    qDebug() << QString("Starting as %1").arg(name);

    ClientApplication *app = new ClientApplication(name, "192.168.1.237", 19501);

    app->mainFunc();

    return 0;
}

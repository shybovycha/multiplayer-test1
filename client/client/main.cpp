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

#define PLAYER_UPDATE_DELAY 0.5

class Player
{
public:
    Player() : position(sf::Vector2f(0, 0))
    {
        previousPosition = position;
    }

    Player(const sf::Vector2f &pos) : position(pos)
    {
        previousPosition = position;
    }

    void update()
    {
        double t = lastUpdateTime / updateTimer.getElapsedTime().asMilliseconds();
        position = lerp(previousPosition, position, t);
    }

    void setPosition(const sf::Vector2f &pos)
    {
        this->previousPosition = this->position;
        this->position = pos;
        this->lastUpdateTime = this->updateTimer.getElapsedTime().asMilliseconds();
        this->updateTimer.restart();
    }

//    void draw()
//    {
//        sf::CircleShape shape(10.0);

//        shape.setPosition(this->position);
//        shape.setFillColor(sf::Color(150, rand() % 255, rand() % 255));

//        window->draw(shape);
//    }

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

QMap<QString, Player*> players;

QString receiveData(sf::TcpSocket *socket)
{
    char in[1000];
    std::size_t received;

    if (socket->receive(in, sizeof(in), received) != sf::Socket::Done)
        return QString("SHUTDOWN");

    return QString(in);
}

sf::Socket::Status sendData(sf::TcpSocket *socket, QString message)
{
    const char* data = message.toStdString().c_str();

    return socket->send(data, message.size() * sizeof(char));
}

sf::Socket::Status sendMainPlayerString(sf::TcpSocket *socket)
{
    // Send player data to the server
    QString out = QString("[%1];%2;%3.").arg(players.firstKey()).arg(players.first()->getPosition().x).arg(players.first()->getPosition().y);

    return sendData(socket, out);
}

void clearPlayers()
{
    while (players.size() > 1)
    {
        players.remove(players.keys().at(1));
    }
}

void createPlayer(QString name, const sf::Vector2f &position)
{
    if (players.contains(name))
    {
        qDebug() << QString("Could not create player `%1`. This name has been taken already").arg(name);
        return;
    }

    Player *player = new Player(position);
    players[name] = player;
}

void updatePlayerPos(QString name, const sf::Vector2f &position)
{
    if (!players.contains(name))
    {
        qDebug() << QString("Could not update player `%1`. This name is not registered").arg(name);
        return;
    }

    players[name]->setPosition(position);
}

void parsePlayerString(const QString &string, QString *playerName, sf::Vector2f *pos)
{
    QRegExp playerRe("\\[([^]]+)\\];(\\d+\\.?\\d*);(\\d+\\.?\\d*)\\.");

    if (playerRe.indexIn(string) < 0)
    {
        qDebug() << QString("`%1` is a bad player string").arg(string);
        return;
    }

    float x = playerRe.cap(2).toFloat(), y = playerRe.cap(3).toFloat();

    *playerName = playerRe.cap(1);
    *pos = sf::Vector2f(x, y);
}

void parsePlayerList(const QString &string)
{
    QRegExp listRe("(\\d+).(\\[([^]]+)\\];(\\d+\\.?\\d*);(\\d+\\.?\\d*)\\.)+");
    QRegExp playerRe("\\[([^]]+)\\];(\\d+\\.?\\d*);(\\d+\\.?\\d*)\\.");

    if (listRe.indexIn(string) < 0)
    {
        qDebug() << QString("`%1` is not a valid player list string").arg(string);
        return;
    }

    int playerCnt = listRe.cap(1).toInt();

    qDebug() << QString("Trying to load %1 players").arg(playerCnt);

    clearPlayers();

    // QStringList playerStrings = listRe.cap(2).split(playerRe);

    // for (int i = 0; i < playerStrings.size(); i++)
    QString listString = listRe.cap(2);
    int prevMatchPos = -1;

    while ((prevMatchPos = playerRe.indexIn(listString, prevMatchPos)) > -1)
    {
        QString name;
        sf::Vector2f pos;

        parsePlayerString(playerRe.cap(0), &name, &pos);

        createPlayer(name, pos);
    }

    qDebug() << QString("Loaded %1 players").arg(players.size());
}

void parseUpdatePlayerString(const QString &string)
{
    QString name;
    sf::Vector2f pos;

    parsePlayerString(string, &name, &pos);

    updatePlayerPos(name, pos);
}

void createMainPlayer(const QString &name)
{
    players.clear();

    Player *player = new Player();
    players[name] = player;
}

sf::Socket::Status sendIntroductionMessage(sf::TcpSocket *socket)
{
    QString name = QString("player-%1").arg(rand() % 255);

    createMainPlayer(name);

    // Send player data to the server
    QString out = QString("<%1>").arg(name);

    return sendData(socket, out);
}


void sendingFunc(sf::TcpSocket *socket)
{
    sf::Clock delayTimer;

    while (true)
    {
        if (delayTimer.getElapsedTime().asSeconds() < PLAYER_UPDATE_DELAY)
            continue;

        if (sendMainPlayerString(socket) != sf::Socket::Done)
        {
            qDebug() << QString("Server has gone away...");
            return;
        }
    }
}

void receivingFunc(sf::TcpSocket *socket)
{
    // send welcome message
    if (sendIntroductionMessage(socket) != sf::Socket::Done)
    {
        qDebug() << QString("Server did not welcome us...");
        return;
    }

    // get initial game state
    QString playerList = receiveData(socket);
    parsePlayerList(playerList);

    sf::Thread sendingThread(&sendingFunc, socket);

    sendingThread.launch();

    while (true)
    {
        // parse player data from server
        QString playerString = receiveData(socket);

        if (playerString == "SHUTDOWN")
        {
            qDebug() << QString("Server has gone away.");
            sendingThread.terminate();
            return;
        }

        parseUpdatePlayerString(playerString);

        // qDebug() << QString("Message received from the server: `%1`").arg(playerString);
    }
}

void handleUserInput()
{
    sf::Vector2f pos = players.first()->getPosition();

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
        pos += sf::Vector2f(0, -1);

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
        pos += sf::Vector2f(0, 1);

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
        pos += sf::Vector2f(-1, 0);

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
        pos += sf::Vector2f(1, 0);

    players.first()->setPosition(pos);
}

int main()
{
    // Ask for the server address
    sf::IpAddress server = "192.168.1.237";
    unsigned short port = 19501;

    // Create a socket for communicating with the server
    sf::TcpSocket socket;

    // Connect to the server
    if (socket.connect(server, port) != sf::Socket::Done)
        return 0;

    qDebug() << QString("Connected to server");

    // create the window
    sf::RenderWindow window(sf::VideoMode(800, 600), "Multiplayer Client");

    sf::Thread receivingThread(&receivingFunc, &socket);

    receivingThread.launch();

    while (window.isOpen())
    {
        // check all the window's events that were triggered since the last iteration of the loop
        sf::Event event;

        while (window.pollEvent(event))
        {
            // "close requested" event: we close the window
            if (event.type == sf::Event::Closed)
                window.close();
        }

        // clear the window with black color
        window.clear(sf::Color::Black);

        for (int i = 0; i < players.size(); i++)
        {
            Player *player = players.values().at(i);

            if (i > 0)
                player->update();

            sf::CircleShape shape(10.0);

            shape.setFillColor(sf::Color(rand() % 255, rand() % 255, rand() % 255));
            shape.setOutlineThickness(4.0);
            shape.setOutlineColor(sf::Color(50, 200, 25));
            shape.setPosition(player->getPosition());

            window.draw(shape);
        }

        handleUserInput();

        // end the current frame
        window.display();
    }

    return 0;
}

#include "singleinstance.h"

#include <QCoreApplication>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>

namespace {

// One name per user, not per machine: two people signed into the same PC each
// get their own player, which is the only reading of "single instance" that
// does not break fast user switching.
QString instanceName()
{
    return QStringLiteral("SqeezeAmp-instance-")
           + QString::fromLocal8Bit(qgetenv("USERNAME")).toLower();
}

constexpr int kConnectTimeoutMs = 500;

// A message is a verb and nothing else, so a sane upper bound on how much a
// client may send before it is ignored costs nothing and bounds the buffer.
constexpr int kMaxMessageBytes = 64;

} // namespace

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
    , m_name(instanceName())
{
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *client = m_server->nextPendingConnection()) {
            connect(client, &QLocalSocket::readyRead, client, [this, client] {
                const Command command = parseCommand(client->read(kMaxMessageBytes));
                switch (command) {
                case Command::Activate:
                    Q_EMIT activationRequested();
                    break;
                case Command::Unknown:
                    // Silently ignored on purpose. The pipe is reachable by
                    // anything running as this user, and an unknown verb is far
                    // more likely to be a typo in somebody's script than
                    // something worth a dialog in front of the music.
                    break;
                default:
                    Q_EMIT commandReceived(command);
                    break;
                }
                client->disconnectFromServer();
            });
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        }
    });
}

SingleInstance::Command SingleInstance::parseCommand(const QByteArray &raw)
{
    // Truncate at the first NUL before trimming: QByteArray::trimmed() strips
    // ASCII whitespace and nothing else, and a client that sizes its buffer for
    // the terminator and writes the lot is the normal shape of a C or scripting
    // caller — not something to answer with silence.
    QByteArray verb = raw;
    if (const qsizetype nul = verb.indexOf('\0'); nul >= 0)
        verb.truncate(nul);

    verb = verb.trimmed().toLower();

    if (verb == "activate")
        return Command::Activate;
    if (verb == "playpause" || verb == "play-pause")
        return Command::PlayPause;
    if (verb == "next")
        return Command::Next;
    if (verb == "previous" || verb == "prev")
        return Command::Previous;
    if (verb == "stop")
        return Command::Stop;

    return Command::Unknown;
}

QByteArray SingleInstance::encodeCommand(Command command)
{
    switch (command) {
    case Command::Activate:  return QByteArrayLiteral("activate");
    case Command::PlayPause: return QByteArrayLiteral("playpause");
    case Command::Next:      return QByteArrayLiteral("next");
    case Command::Previous:  return QByteArrayLiteral("previous");
    case Command::Stop:      return QByteArrayLiteral("stop");
    case Command::Unknown:   break;
    }
    return {};
}

bool SingleInstance::claim(Command commandIfRunning)
{
    // Is somebody already there? Connecting is also how the running instance
    // is told to act, so the two questions are one round trip.
    QLocalSocket probe;
    probe.connectToServer(m_name);
    if (probe.waitForConnected(kConnectTimeoutMs)) {
        probe.write(encodeCommand(commandIfRunning));
        probe.waitForBytesWritten(kConnectTimeoutMs);
        probe.disconnectFromServer();
        return false;
    }

    // Nobody answered. A stale pipe can survive a crash on some platforms and
    // would make every future launch think an instance is running; removing it
    // is safe precisely because the connect above just failed.
    QLocalServer::removeServer(m_name);

    if (!m_server->listen(m_name)) {
        // Failing to listen is not a reason to refuse to start. The worst case
        // is that a second launch opens a second window, which is a nuisance;
        // refusing to open the first one is not.
        return true;
    }

    // QLocalServer on Windows is a named pipe, so this creates no socket of
    // any kind. prd.md NFR-10 is checked with netstat against the app's PID,
    // and this line is the reason that check passes.
    return true;
}

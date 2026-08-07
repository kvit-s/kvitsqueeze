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

constexpr const char *kActivate = "activate";
constexpr int kConnectTimeoutMs = 500;

} // namespace

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
    , m_name(instanceName())
{
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *client = m_server->nextPendingConnection()) {
            connect(client, &QLocalSocket::readyRead, client, [this, client] {
                if (client->readAll().startsWith(kActivate))
                    Q_EMIT activationRequested();
                client->disconnectFromServer();
            });
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        }
    });
}

bool SingleInstance::claim()
{
    // Is somebody already there? Connecting is also how the running instance
    // is told to show itself, so the two questions are one round trip.
    QLocalSocket probe;
    probe.connectToServer(m_name);
    if (probe.waitForConnected(kConnectTimeoutMs)) {
        probe.write(kActivate);
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

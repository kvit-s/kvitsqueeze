#include "serverbrowser.h"

#include <QHostAddress>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTimer>
#include <QUdpSocket>

namespace {

constexpr quint16 kDiscoveryPort = 3483;
constexpr int kRepeatMs = 700;

} // namespace

ServerBrowser::ServerBrowser(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
    , m_repeat(new QTimer(this))
    , m_deadline(new QTimer(this))
{
    // Port 0: the OS picks an ephemeral source port and the replies come back
    // to it. Binding 3483 would be the listening socket prd.md N7 forbids, and
    // would also collide with a squeezelite running on this machine.
    m_socket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress);
    connect(m_socket, &QUdpSocket::readyRead, this, &ServerBrowser::readReplies);

    m_repeat->setInterval(kRepeatMs);
    connect(m_repeat, &QTimer::timeout, this, &ServerBrowser::broadcast);

    m_deadline->setSingleShot(true);
    connect(m_deadline, &QTimer::timeout, this, [this] {
        m_repeat->stop();
        Q_EMIT scanFinished();
    });
}

bool ServerBrowser::isScanning() const
{
    return m_repeat->isActive();
}

void ServerBrowser::broadcast()
{
    const QByteArray request = ServerDiscovery::requestDatagram();
    m_socket->writeDatagram(request, QHostAddress::Broadcast, kDiscoveryPort);

    // The global broadcast address does not leave the interface the routing
    // table picks, so each interface's own directed broadcast is sent as well.
    // On a multi-homed machine — a VPN, a Hyper-V switch, WiFi plus Ethernet —
    // that is the difference between finding the server and finding nothing.
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        if (!interface.flags().testFlag(QNetworkInterface::IsUp)
            || !interface.flags().testFlag(QNetworkInterface::CanBroadcast))
            continue;
        const QList<QNetworkAddressEntry> entries = interface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            if (!entry.broadcast().isNull())
                m_socket->writeDatagram(request, entry.broadcast(), kDiscoveryPort);
        }
    }
}

void ServerBrowser::scan(int durationMs)
{
    m_seen.clear();
    // The first probe goes out now rather than one interval from now: a scan
    // that appears to do nothing for most of a second reads as broken.
    broadcast();
    m_repeat->start();
    m_deadline->start(durationMs);
}

void ServerBrowser::probe(const QString &host)
{
    m_socket->writeDatagram(ServerDiscovery::requestDatagram(),
                            QHostAddress(host), kDiscoveryPort);
}

void ServerBrowser::readReplies()
{
    while (m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram();
        DiscoveredServer server = ServerDiscovery::parseReply(datagram.data());
        if (!server.isValid())
            continue;

        // The datagram's sender is the address that actually works. The IPAD
        // tag is what the server believes its own address to be, which behind
        // a container bridge — exactly the Home Assistant add-on case — is an
        // address this machine cannot reach.
        server.address = datagram.senderAddress().toString();

        const QString key = server.address + QLatin1Char(':')
                            + QString::number(server.jsonPort);
        if (m_seen.contains(key))
            continue;
        m_seen.append(key);

        Q_EMIT serverFound(server);
    }
}

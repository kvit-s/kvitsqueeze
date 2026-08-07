#pragma once

// prd.md FR-1.1: find Lyrion Music Server instances by UDP broadcast on 3483.
//
// The socket half of ServerDiscovery — the datagram grammar itself is a pure
// function in sqz-protocol and is tested there.
//
// Expect this to find nothing on a routed network. The server this app was
// developed against is a Home Assistant add-on on another subnet, and it
// answered a unicast probe while ignoring the broadcast, which is prd.md §13
// Q6 answered: **manual entry (FR-1.2) is the reliable path and discovery is
// the convenience.** So a scan that returns an empty list is a normal outcome
// the settings screen has to present without looking broken.
//
// This is an outbound datagram and a bound *client* socket for the replies —
// not a listening service. Nothing on the network can make this app do
// anything (prd.md N7 / NFR-10).

#include "serverdiscovery.h"

#include <QList>
#include <QObject>

class QTimer;
class QUdpSocket;

class ServerBrowser : public QObject
{
    Q_OBJECT

public:
    explicit ServerBrowser(QObject *parent = nullptr);

    // Broadcast a few times over `durationMs` and report what answers. Several
    // probes rather than one because a single UDP datagram is allowed to
    // vanish and the user reads an empty list as "there is no server".
    void scan(int durationMs = 2500);

    // Ask one address directly. This is what makes a manually-typed host fill
    // in its own name and version, and the only form that works across a
    // subnet boundary.
    void probe(const QString &host);

    bool isScanning() const;

Q_SIGNALS:
    void serverFound(const DiscoveredServer &server);
    void scanFinished();

private:
    void readReplies();
    void broadcast();

    QUdpSocket *m_socket = nullptr;
    QTimer *m_repeat = nullptr;
    QTimer *m_deadline = nullptr;
    QList<QString> m_seen;
};

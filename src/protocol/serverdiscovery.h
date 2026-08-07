#pragma once

// The UDP discovery exchange on port 3483 (prd.md FR-1.1), as two pure
// functions: build the question, read the answer. The socket that carries them
// lives in sqz-session.
//
// LMS answers two dialects on the same port. The old one is a single 'd' and
// gets back 'D' plus a padded server name — enough to know something is there
// and nothing else. The TLV dialect is the useful one:
//
//   request   'e' then, per tag, four ASCII bytes and a zero length byte
//   reply     'E' then, per tag, four ASCII bytes, one length byte, the value
//
// Confirmed against Lyrion Music Server 9.1.0, which answered a unicast probe
// with:
//
//   ENAME\x05Media JSON\x049000 VERS\x059.1.0 UUID\x24<uuid>
//
// The same server did **not** answer a broadcast from this machine, which is
// prd.md §13 Q6 settled the way FR-1.2 already assumed: the server is a Home
// Assistant add-on on another subnet, so manual entry is the reliable path and
// discovery is the convenience.

#include <QByteArray>
#include <QString>

struct DiscoveredServer
{
    QString name;
    QString address;          // filled in by the caller from the datagram's sender
    quint16 jsonPort = 9000;  // the control API's port, from the JSON tag
    QString version;
    QString uuid;

    bool isValid() const { return !name.isEmpty(); }
};

namespace ServerDiscovery {

// The tags worth asking for. IPAD is included because older servers answer it
// with their own address, which is the only way to learn the address when the
// reply arrives through a relay rather than direct.
QByteArray requestDatagram();

// Parse an 'E' reply. Returns an invalid record for anything else, including
// the old 'D' dialect — knowing a server exists without knowing its control
// port is not enough to connect to it.
DiscoveredServer parseReply(const QByteArray &datagram);

} // namespace ServerDiscovery

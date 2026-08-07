#include "serverdiscovery.h"

namespace ServerDiscovery {

QByteArray requestDatagram()
{
    static const char *tags[] = { "NAME", "IPAD", "JSON", "VERS", "UUID" };

    QByteArray datagram("e");
    for (const char *tag : tags) {
        datagram.append(tag, 4);
        datagram.append('\0'); // a zero-length value: this is the question form
    }
    return datagram;
}

DiscoveredServer parseReply(const QByteArray &datagram)
{
    DiscoveredServer server;
    if (datagram.size() < 6 || datagram.at(0) != 'E')
        return server;

    int offset = 1;
    while (offset + 5 <= datagram.size()) {
        const QByteArray tag = datagram.mid(offset, 4);
        const int length = static_cast<quint8>(datagram.at(offset + 4));
        offset += 5;

        // A length that runs off the end means the datagram was truncated.
        // Stopping keeps whatever was read before it rather than discarding a
        // usable name and port over a trailing field.
        if (offset + length > datagram.size())
            break;

        const QString value = QString::fromUtf8(datagram.mid(offset, length));
        offset += length;

        if (tag == "NAME")
            server.name = value;
        else if (tag == "VERS")
            server.version = value;
        else if (tag == "UUID")
            server.uuid = value;
        else if (tag == "IPAD")
            server.address = value;
        else if (tag == "JSON") {
            bool ok = false;
            const uint port = value.toUInt(&ok);
            if (ok && port > 0 && port <= 65535)
                server.jsonPort = static_cast<quint16>(port);
        }
    }

    return server;
}

} // namespace ServerDiscovery

// SPDX-License-Identifier: MPL-2.0

#include "playeridentity.h"

#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>

namespace {

// One cached value for the life of the process. Regenerating mid-run would
// register a second player on the server and orphan the first one's queue.
QString g_mac;

const char *kSettingsKey = "player/id";

} // namespace

QString PlayerIdentity::generate()
{
    quint64 bits = QRandomGenerator::global()->generate64();

    quint8 octets[6];
    for (int i = 0; i < 6; ++i)
        octets[i] = static_cast<quint8>((bits >> (i * 8)) & 0xff);

    octets[0] = static_cast<quint8>((octets[0] | 0x02) & 0xfe);

    QString mac;
    for (int i = 0; i < 6; ++i) {
        if (i)
            mac += QLatin1Char(':');
        mac += QString::asprintf("%02x", octets[i]);
    }
    return mac;
}

bool PlayerIdentity::isValid(const QString &mac)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}$"));
    return pattern.match(mac).hasMatch();
}

QString PlayerIdentity::load()
{
    QSettings settings;
    const QString stored = settings.value(QLatin1String(kSettingsKey)).toString();
    if (isValid(stored))
        return stored;

    // Either first run, or the key was hand-edited into something squeezelite
    // would reject at -m. Replacing it loses the server-side queue once;
    // leaving it would fail to register a player at all.
    const QString fresh = generate();
    settings.setValue(QLatin1String(kSettingsKey), fresh);
    settings.sync();
    return fresh;
}

QString PlayerIdentity::mac()
{
    if (g_mac.isEmpty())
        g_mac = load();
    return g_mac;
}

void PlayerIdentity::overrideForTesting(const QString &mac)
{
    g_mac = mac;
}

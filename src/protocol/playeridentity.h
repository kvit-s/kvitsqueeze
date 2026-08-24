// SPDX-License-Identifier: MPL-2.0

#pragma once

// Who this player is, as one process-wide fact.
//
// prd.md FR-1.4 wants a stable, persisted, MAC-style id: the same id across
// restarts is what keeps the queue and the per-player settings server-side.
// Two modules need it and neither may learn it from the other — LmsSession
// injects it into every control request (FR-6.1), and ExternalEngine passes it
// to squeezelite's -m. sqz-app and sqz-qml are forbidden from touching a
// player id at all (tools/check-layering.py), so there is no module in the
// graph that could carry the value from one to the other.
//
// So both read it here instead. A singleton is the right shape rather than a
// concession: prd.md N6/D3 settles that one process is exactly one player,
// permanently, and explicitly allows code to rely on it.
//
// This is the one type in sqz-protocol that touches storage. "Pure" here means
// no sockets and no GUI — the property that lets the request and reply code be
// tested headless — and a QSettings key does not cost that. generate() and
// isValid() stay pure functions so the interesting half is still testable.

#include <QString>

class PlayerIdentity
{
public:
    // The process's player id, loaded on first use and generated + persisted
    // if this machine has never run KvitSqueeze before.
    static QString mac();

    // A locally-administered, unicast MAC: the second-least-significant bit of
    // the first octet set, the least-significant clear. Those two bits are
    // what tell the world this address was made up rather than assigned, so a
    // generated id cannot collide with a real NIC — or with a real Squeezebox,
    // whose addresses all come out of Logitech's 00:04:20 range.
    static QString generate();

    static bool isValid(const QString &mac);

    // Test seam. Replaces the cached value without writing it to disk, so a
    // test can pin the id without touching the developer's own settings.
    static void overrideForTesting(const QString &mac);

private:
    static QString load();
};

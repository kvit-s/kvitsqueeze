#pragma once

// What arrives on the CLI event socket, and what it means.
//
// prd.md §13 Q1 is settled: **CLI on port 9090** carries the push stream.
// Checked against Lyrion Music Server 9.1.0 by opening the socket, sending
// `listen 1`, and driving playback from the server's own web UI — every
// transport, queue, volume and power change came back, which is prd.md §14
// assumption 2 and the only thing that could have flipped the decision to
// CometD.
//
// What arrives is the *command that was executed*, not a state snapshot:
//
//   00%3A04%3A20%3A2a%3Acc%3A0f mixer volume %2B0
//   00%3A04%3A20%3A2a%3Acc%3A0f pause 1
//
// So an event is a hint that something changed, and the authoritative state
// still comes from a `status` request. That is a feature rather than a cost:
// there is exactly one code path that produces a PlayerStatus, so a
// notification cannot introduce a state the polling path could not.
//
// The first token is the player the event is about — which is what makes
// prd.md FR-6.2 enforceable at the session layer rather than by discipline.

#include <QString>
#include <QStringList>

struct CliEvent
{
    // Empty for a server-scoped line (`rescan done`, the reply to `version ?`).
    QString playerId;

    // The command, player id already removed and every token decoded.
    QStringList tokens;

    QString verb() const { return tokens.value(0); }

    // Does this event say the player's state may have moved? Used to decide
    // whether to spend a `status` round trip. Deliberately generous: a missed
    // refresh shows stale transport state until the next poll, while a
    // needless one costs a few hundred bytes on a LAN.
    bool affectsPlayerState() const;

    // Does it say the *queue* changed, as opposed to just transport? A queue
    // refetch is the expensive one, so it is asked separately.
    bool affectsQueue() const;

    static CliEvent parse(const QByteArray &line);
};

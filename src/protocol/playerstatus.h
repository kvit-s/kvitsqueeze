#pragma once

// One parsed snapshot of what the server says our player is doing.
//
// The server is the single source of truth for transport state, queue and
// volume (prd.md §7.4). The UI applies an optimistic local update on a user
// action and then reconciles against the next authoritative snapshot, so this
// type is the thing being reconciled *to* — it must never be constructed from
// a local guess.

#include <QJsonObject>
#include <QString>

struct PlayerStatus
{
    enum class Mode { Stopped, Playing, Paused };

    Mode mode = Mode::Stopped;
    bool powered = true;

    // Position and duration in seconds. Negative means the server did not
    // report one — a radio stream has no duration, and treating that as 0
    // makes a seek bar claim the track has ended.
    double elapsed = -1.0;
    double duration = -1.0;

    int volume = -1;            // 0-100, or -1 for unknown
    int playlistIndex = -1;     // current position in the queue
    int playlistCount = 0;

    QString title;
    QString artist;
    QString album;
    QString coverId;            // builds the artwork URL; may be empty

    // Parse the `result` object of a `status` reply. A malformed or partial
    // reply yields a default-constructed status rather than throwing: the
    // connection layer decides what a bad reply means, and a half-populated
    // snapshot reaching a model is worse than an empty one.
    static PlayerStatus fromStatusResult(const QJsonObject &result);
};

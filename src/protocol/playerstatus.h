#pragma once

// One parsed snapshot of what the server says our player is doing.
//
// The server is the single source of truth for transport state, queue and
// volume (prd.md §7.4). The UI applies an optimistic local update on a user
// action and then reconciles against the next authoritative snapshot, so this
// type is the thing being reconciled *to* — it must never be constructed from
// a local guess.
//
// Note what is deliberately absent: any other player. A `status` reply can
// carry sync_master and sync_slaves, and those are read here only far enough
// to answer "is this player in somebody's sync group" as a bool. Keeping the
// foreign ids out of the struct is what makes prd.md FR-6.2 structural — a
// model cannot display a player list it was never handed (FR-6.5 asks for a
// passive indicator, and a bool is the whole of it).

#include <QJsonObject>
#include <QList>
#include <QString>

// One row of the server's copy of our queue.
struct QueueTrack
{
    QString id;
    QString title;
    QString artist;
    QString album;
    QString albumId;
    QString coverId;
    double duration = -1.0;
};

struct PlayerStatus
{
    enum class Mode { Stopped, Playing, Paused };

    // False until a reply has actually been parsed. Distinguishes "the player
    // is stopped" from "we have never heard from the server", which look
    // identical in every other field and mean opposite things to the
    // connection banner (prd.md FR-1.5).
    bool valid = false;

    Mode mode = Mode::Stopped;
    bool powered = true;
    bool connected = false;

    // Position and duration in seconds. Negative means the server did not
    // report one — a radio stream has no duration, and treating that as 0
    // makes a seek bar claim the track has ended.
    double elapsed = -1.0;
    double duration = -1.0;

    int volume = -1;            // 0-100, or -1 for unknown
    bool muted = false;
    int playlistIndex = -1;     // current position in the queue
    int playlistCount = 0;

    int repeat = 0;             // 0 off, 1 one, 2 all
    int shuffle = 0;            // 0 off, 1 songs, 2 albums

    // Bumped by the server whenever the queue's contents change. Comparing it
    // is what keeps a 500-track queue from being refetched on every track
    // boundary (prd.md FR-3.2's spirit, applied to the queue).
    double playlistTimestamp = 0.0;

    // prd.md FR-6.5: the app never creates, joins or leaves a sync group, but
    // an external controller may put it in one, and the UI says so passively.
    bool synced = false;

    QString playerName;

    QString title;
    QString artist;
    QString album;
    QString coverId;            // builds the artwork URL; may be empty
    QString trackId;

    // The window of the queue this reply carried, and where it started. A
    // `status - 1` reply carries only the current track, so the queue is left
    // empty rather than being replaced by a one-row version of itself.
    QList<QueueTrack> queue;
    int queueStart = 0;
    bool queueIncluded = false;

    // Parse the `result` object of a `status` reply. A malformed or partial
    // reply yields a default-constructed status rather than throwing: the
    // connection layer decides what a bad reply means, and a half-populated
    // snapshot reaching a model is worse than an empty one.
    static PlayerStatus fromStatusResult(const QJsonObject &result);
};

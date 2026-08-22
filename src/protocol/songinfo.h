#pragma once

// What `songinfo` says about one track, and the parsing that turns its reply
// into it (prd.md FR-5.5).
//
// The reply's shape is the reason this needs a parser at all. Every other
// query in this app returns a loop of records; `songinfo` returns a loop of
// *fields* — one single-key object per tag, in whatever order the server feels
// like — so the caller has to flatten it before it can read anything:
//
//   {"songinfo_loop":[{"id":10125},{"title":"#1 Track"},{"lyrics":"the first line…"}]}
//
// Verified against Lyrion Music Server 9.1.0. The tag letter for lyrics is
// `w`; `R`, `k` and `z` return nothing of the sort, which is worth writing
// down because the LMS tag table is a single letter per field and a wrong
// guess fails silently as "this file has none".
//
// Lyrics come from the file's own tags. LMS reads them at scan time and serves
// what it found; there is no lookup service behind this, and a file without an
// embedded lyric sheet has none as far as the server is concerned — which is
// prd.md N4 holding: no plugin is involved in answering this.

#include <QJsonObject>
#include <QString>

struct SongInfo
{
    QString trackId;
    QString title;

    // Where the server keeps the file — its own path, not one this machine can
    // necessarily open. It is what a `.lrc` sidecar is located from.
    QString url;

    // Empty means the file carries no lyric tag — *if* `answered` is true.
    QString lyrics;

    // Whether the server actually described a track. Without it an empty
    // `lyrics` is ambiguous between "this file has none" and "nobody has said
    // yet", and the second is not something to draw as the first (prd.md
    // FR-2.5's rule, applied to metadata).
    bool answered = false;

    static SongInfo fromResult(const QJsonObject &result);
};

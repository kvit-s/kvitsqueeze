// SPDX-License-Identifier: MPL-2.0

#pragma once

// A timed lyric sheet, and the two pure functions that produce one (prd.md
// FR-5.5).
//
// LMS does not serve these. `songinfo tags:w` returns the file's *unsynchronised*
// lyric tag — ID3's USLT, plain text with no timing — and that is all the
// server knows: it does not read `.lrc` sidecars, and it does not expose SYLT
// either. Checked against Lyrion Music Server 9.1.0 over 400 tracks: not one
// carried a timestamp, while 1109 `.lrc` files sat beside those same mp3s on
// the share, every one of them paired by basename.
//
// So a timed sheet comes from the file next to the track, read by this app,
// and an untimed one comes from the server. Both draw the same way; only one
// of them knows which line is being sung.
//
// The format is the usual one, and the parts of it that actually occur:
//
//   [00:23.058]the first line          a line, at 23.058 s
//   [00:12.00][01:30.00]same line      a refrain, timed twice
//   [ar:Artist] [ti:Track]             metadata, discarded
//   [offset:+250]                      a global shift, in milliseconds
//   [00:00.000]...                     a placeholder line, kept as it is
//
// Nothing here reads a file or knows what a track is: it takes text and gives
// back lines, so the whole of it is testable without a share to mount.

#include <QList>
#include <QString>
#include <QStringList>

struct LrcSheet
{
    struct Line
    {
        // When the line is sung. Never negative: an offset that would push the
        // first line before the start of the track clamps to zero.
        double seconds = 0.0;
        QString text;
    };

    QList<Line> lines;

    bool isEmpty() const { return lines.isEmpty(); }

    // The text alone, in order, for a view that draws the sheet as lines.
    QStringList texts() const;

    // Which line is being sung at `seconds`, or -1 before the first one.
    // Binary search: this is called on every position tick.
    int lineAt(double seconds) const;

    // Whether `text` looks like a timed sheet at all. A `.lrc` file whose
    // timestamps are missing or malformed is not a timed sheet, and pretending
    // it is would put the highlight on line 0 for the whole track.
    static bool looksTimed(const QString &text);

    static LrcSheet parse(const QString &text);
};

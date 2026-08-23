// SPDX-License-Identifier: MPL-2.0

#pragma once

// Where a track's `.lrc` file would be, as this PC sees it (prd.md FR-5.5).
//
// The server reports a track's location as its own path — `file:///data/mnt/
// music/albums/…` on the Lyrion host — which means nothing to a Windows
// client. So the app is told once where the same music lives from here (a
// drive, a UNC share, a local folder) and matches the tail of the server's
// path against it:
//
//   server   file:///data/mnt/music/albums/Example%20-%20%231%20Track.mp3
//   setting  \\MUSICNAS\music
//   tried    \\MUSICNAS\music\data\mnt\music\albums\Example - #1 Track.lrc
//            \\MUSICNAS\music\mnt\music\albums\…
//            \\MUSICNAS\music\music\albums\…
//            \\MUSICNAS\music\albums\Example - #1 Track.lrc     ← this one
//            \\MUSICNAS\music\Example - #1 Track.lrc
//
// Longest tail first, so a root holding two `albums` folders at different
// depths resolves to the deeper match rather than the first plausible one.
// Trying every tail is what lets one setting work whether the user points at
// the music root or at the folder the tracks are actually in.
//
// Pure: it builds candidate paths and touches nothing. Whether any of them
// exists is the caller's problem, off the GUI thread — a UNC path on a
// sleeping NAS can take seconds to answer.

#include <QString>
#include <QStringList>

namespace LyricsSidecar {

// Empty when there is no local root configured, when the track is not a local
// file on the server either (a stream has no sidecar), or when the path has no
// filename to work from.
QStringList candidates(const QString &trackUrl, const QString &localRoot);

} // namespace LyricsSidecar

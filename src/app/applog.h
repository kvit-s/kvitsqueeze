#pragma once

// prd.md FR-9.1: a rotating log file under %LOCALAPPDATA%\SqeezeAmp\logs\,
// with per-subsystem levels.
//
// Built on Qt's own logging categories rather than a parallel mechanism, so
// every qCDebug in the tree lands here and the level rules are the ones Qt
// already understands. The subsystems are the modules:
//
//     sqz.protocol   request building and reply parsing
//     sqz.session    transport, the event stream, reconnect
//     sqz.engine     the child process and what it says
//     sqz.ui         models and the shell
//
// Rotation is by size with a small number of generations. A player left
// running for a week must not fill a disk, and a bug report needs the last few
// minutes rather than the whole history.

#include <QLoggingCategory>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(logProtocol)
Q_DECLARE_LOGGING_CATEGORY(logSession)
Q_DECLARE_LOGGING_CATEGORY(logEngine)
Q_DECLARE_LOGGING_CATEGORY(logUi)

namespace AppLog {

// Call once, from main(), before anything logs. Installs the message handler
// and opens the file; failing to open the file is not fatal, because a player
// that will not start because it could not write a log is worse than a player
// with no log.
void install();

// Where the file is, for the "copy diagnostics" button and the settings
// screen's "open log folder".
QString directory();
QString currentFile();

} // namespace AppLog

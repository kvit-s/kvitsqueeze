// SPDX-License-Identifier: MPL-2.0

#pragma once

// prd.md FR-7.3: a second launch raises the existing window and exits.
//
// **A named pipe, never a TCP port — not even on loopback.** prd.md N7 makes
// this the single most load-bearing implementation detail in the Windows
// integration: the app must hold no listening socket on any interface, and
// NFR-10 puts a `netstat -ano` check against the release checklist.
// QLocalServer on Windows is a named pipe, which is why it is the right
// primitive — and why the "simpler" loopback-listener version of this class
// would be a requirement violation rather than a shortcut.
//
// This is also the only listening endpoint the process has at all. Everything
// else is outbound (prd.md N7).
//
// The pipe carries a closed vocabulary of transport verbs as well as
// `activate` (prd.md FR-7.10), so a scripted key on this machine can drive
// playback without the app growing a control surface. Two properties are what
// keep that inside N7: the pipe is per-user and stays a pipe, and a verb names
// only an action. Nothing here may learn to carry a player id, a server
// address or a query — the moment it does, this is a remote-control API and
// N7 is gone.

#include <QByteArray>
#include <QObject>
#include <QString>

class QLocalServer;

class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);

    // The whole vocabulary. Deliberately an enum rather than a passed-through
    // string: a verb that is not on this list cannot reach the player, and
    // adding one is a visible edit here rather than a new string somewhere.
    enum class Command {
        Unknown,
        Activate,
        PlayPause,
        Next,
        Previous,
        Stop,
    };
    Q_ENUM(Command)

    // Static and pure so the vocabulary is testable without a pipe, a window
    // or a running app — same reason ExternalEngine::buildArguments() is.
    // Tolerates surrounding whitespace and any case; anything else is Unknown.
    static Command parseCommand(const QByteArray &raw);
    static QByteArray encodeCommand(Command command);

    // True when this process is the first. False means another instance was
    // told to do `commandIfRunning` and this one should exit — quietly,
    // because for Activate the user asked for a window and they got one.
    bool claim(Command commandIfRunning = Command::Activate);

Q_SIGNALS:
    // Another launch happened. The shell raises and un-minimises its window.
    void activationRequested();

    // A transport verb arrived on the pipe (prd.md FR-7.10). These are wired to
    // the same PlaybackController slots as the media keys, in the same place.
    void commandReceived(Command command);

private:
    QLocalServer *m_server = nullptr;
    QString m_name;
};

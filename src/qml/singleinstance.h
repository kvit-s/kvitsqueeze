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

#include <QObject>
#include <QString>

class QLocalServer;

class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);

    // True when this process is the first. False means another instance was
    // told to show itself and this one should exit — quietly, because the user
    // asked for a window, and they got one.
    bool claim();

Q_SIGNALS:
    // Another launch happened. The shell raises and un-minimises its window.
    void activationRequested();

private:
    QLocalServer *m_server = nullptr;
    QString m_name;
};

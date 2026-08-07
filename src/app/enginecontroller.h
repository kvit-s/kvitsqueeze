#pragma once

// The audio engine as QML sees it: a state, a few numbers, and a device list.
//
// This is the only thing above sqz-engine that touches an IAudioEngine, and it
// deliberately talks to the *interface* — no ExternalEngine type appears here,
// which is the rule that keeps prd.md §7.3's backend decision reversible.
//
// prd.md FR-2.5's rule is the one to keep in mind while reading the
// properties: a field the backend could not determine is reported as unknown
// and the UI hides it. Under Backend B most of them are scraped from log
// lines, so "unknown" is a normal answer and not an error — what would be
// wrong is a diagnostics panel stating a sample rate of 0 as fact.
//
// Note what is not here: the player id. The engine needs one for squeezelite's
// -m and gets it from PlayerIdentity itself, because prd.md FR-6.1 keeps a
// player id out of sqz-app entirely.

#include "iaudioengine.h"

#include <QObject>
#include <QString>
#include <QStringList>

class Settings;

class EngineController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int state READ state NOTIFY statusChanged)
    Q_PROPERTY(QString stateText READ stateText NOTIFY statusChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY statusChanged)
    Q_PROPERTY(bool available READ isAvailable NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)

    Q_PROPERTY(QString decoder READ decoder NOTIFY statusChanged)
    Q_PROPERTY(int sourceSampleRate READ sourceSampleRate NOTIFY statusChanged)
    Q_PROPERTY(int sourceBitDepth READ sourceBitDepth NOTIFY statusChanged)
    Q_PROPERTY(int outputSampleRate READ outputSampleRate NOTIFY statusChanged)
    Q_PROPERTY(int underruns READ underruns NOTIFY statusChanged)
    Q_PROPERTY(QString activeOutputDevice READ activeOutputDevice NOTIFY statusChanged)

    // The badge on the Now Playing view (prd.md §9.2), already assembled from
    // whichever fields are actually known. Empty when nothing is.
    Q_PROPERTY(QString formatBadge READ formatBadge NOTIFY statusChanged)

    Q_PROPERTY(QStringList deviceNames READ deviceNames NOTIFY devicesChanged)

public:
    EngineController(IAudioEngine *engine, Settings *settings, QObject *parent = nullptr);

    int state() const { return int(m_status.state); }
    QString stateText() const;
    bool isRunning() const { return m_status.state == EngineStatus::State::Running; }
    bool isAvailable() const { return m_available; }
    QString lastError() const { return m_status.lastError; }

    QString decoder() const { return m_status.decoder; }
    int sourceSampleRate() const { return m_status.sourceSampleRate; }
    int sourceBitDepth() const { return m_status.sourceBitDepth; }
    int outputSampleRate() const { return m_status.outputSampleRate; }
    int underruns() const { return m_status.underruns; }
    QString activeOutputDevice() const { return m_status.outputDevice; }
    QString formatBadge() const;
    QStringList deviceNames() const;

    // The SlimProto address. Separate from the control API's host and port
    // because they are different ports on the same machine, and the engine is
    // handed only what it needs (prd.md §7.3).
    void setServerHost(const QString &host);

    // Start or restart with the current settings. Called when the server or
    // any audio setting changes; a change that does not affect the child
    // process does not restart it.
    Q_INVOKABLE void apply();

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void refreshDevices();

Q_SIGNALS:
    void statusChanged();
    void devicesChanged();
    void logLine(const QString &line);

private:
    EngineConfig buildConfig() const;

    IAudioEngine *m_engine = nullptr;
    Settings *m_settings = nullptr;
    EngineStatus m_status;
    EngineConfig m_applied;
    QString m_serverHost;
    bool m_available = false;
    bool m_wanted = false;
};

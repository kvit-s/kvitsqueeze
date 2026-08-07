#pragma once

// Backend B (prd.md §7.3.2): supervise a stock squeezelite.exe child process.
//
// This is the only backend v1 builds. It ships the upstream binary unmodified
// and talks to it only through documented command-line arguments and its log
// output — which is what keeps SqeezeAmp's own licence unconstrained
// (prd.md §11.2). Do not patch the binary, and do not add a private channel
// to it.

#include "iaudioengine.h"

#include <QList>
#include <QProcess>

class QTimer;

class ExternalEngine : public IAudioEngine
{
    Q_OBJECT

public:
    explicit ExternalEngine(QObject *parent = nullptr);
    ~ExternalEngine() override;

    // Path to squeezelite.exe. Set before start(); defaults to
    // engine/squeezelite.exe beside the application, which is where the
    // installer stages it.
    void setExecutablePath(const QString &path);
    QString executablePath() const { return m_executable; }

    // False when the binary is not where it is expected, so the UI can say
    // "the audio engine is missing" instead of failing at the first play.
    bool isAvailable() const override;

    bool start(const EngineConfig &config) override;
    void stop() override;
    bool setOutputDevice(const QString &device) override;
    QList<AudioDevice> devices() const override { return m_devices; }
    void refreshDevices() override;
    EngineStatus status() const override { return m_status; }

    // The argument vector for a config, as a pure function so it can be
    // tested without launching anything. This is the entire interface to the
    // child process, so it is the thing worth pinning down in tests.
    //
    // Exclusive mode is `-a <latency>:<exclusive>` and is reachable from the
    // command line — the risk that would have ruled this backend out, and
    // does not (prd.md §7.3.2).
    static QStringList buildArguments(const EngineConfig &config);

    // Parse `squeezelite -l` output. Separate and static because the format is
    // upstream's and a change in it should fail a test, not a user's settings
    // screen. Lines look like:
    //
    //     Output devices:
    //       11 - Realtek Digital Output (Realtek(R) Audio) [Windows WASAPI]
    static QList<AudioDevice> parseDeviceList(const QString &output);

    // Fold one log line into a status. Static and pure for the same reason:
    // this is the whole of FR-2.5 under Backend B and the log format is not a
    // stable interface, so it needs cases rather than hope. Returns true when
    // something actually changed.
    //
    // The rule that matters is prd.md FR-2.5's: a line that does not match
    // must leave the field *unknown*. Reporting a sample rate of 0 as fact is
    // worse than reporting nothing.
    static bool applyLogLine(const QString &line, EngineStatus *status);

    // What actually reaches `-o`, as a pure function of the three inputs, so
    // the rule has cases instead of a comment.
    //
    // A device the user picked always wins. "System default" — an empty
    // setting — resolves to `systemDefault`, because the engine's own idea of a
    // default is the first MME device and not the one Windows would use. It
    // resolves to nothing at all when that name is not among the devices the
    // engine enumerated: a name the engine cannot open is worse than letting it
    // choose, and prd.md FR-2.3 persists devices by name precisely so a missing
    // one is noticed rather than silently repointed.
    static QString resolveOutputDevice(const QString &configured,
                                       const QString &systemDefault,
                                       const QList<AudioDevice> &known);

private:
    void handleStandardError();
    void setState(EngineStatus::State state, const QString &error = {});
    void publish();
    bool launch();
    void adoptIntoJob();

    QProcess *m_process = nullptr;
    QProcess *m_enumerator = nullptr;
    QTimer *m_restart = nullptr;
    QString m_executable;
    EngineConfig m_config;
    EngineStatus m_status;
    QList<AudioDevice> m_devices;
    QByteArray m_partialLine;
    int m_restartBackoffMs = 1000;
    int m_consecutiveFailures = 0;
    bool m_stopRequested = false;

    // Set when the child refuses to open the resolved system default, so the
    // restart that its exit triggers falls back to the engine's own default
    // instead of failing the same way five times. Cleared by start(), so a
    // settings change or a re-enumeration tries again.
    bool m_systemDefaultRejected = false;
    bool m_launchedWithSystemDefault = false;

    // Windows Job Object with KILL_ON_JOB_CLOSE (prd.md §7.3.2). Held as a
    // void* so this header names no Windows type; the app must never leave a
    // squeezelite behind holding the audio device and a player registration.
    void *m_job = nullptr;
};

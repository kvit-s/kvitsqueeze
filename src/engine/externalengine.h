// SPDX-License-Identifier: MPL-2.0

#pragma once

// Backend B (prd.md §7.3.2): supervise a stock squeezelite.exe child process.
//
// This is the only backend v1 builds. It ships the upstream binary unmodified
// and talks to it only through documented command-line arguments and its log
// output — which is what keeps KvitSqueeze's own licence unconstrained
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

    // Pin the engine to one path, overriding the search below. For tests and
    // for SQZ_ENGINE_EXE; normal runs leave this alone so that an engine
    // arriving later is found without a restart.
    void setExecutablePath(const QString &path);

    // The engine this would launch right now: the first candidate that exists,
    // or the preferred one when none does — so an error message can name a
    // path rather than trail off.
    QString executablePath() const;

    // False when the binary is not where it is expected, so the UI can say
    // "the audio engine is missing" instead of failing at the first play.
    bool isAvailable() const override;

    // Backend B needs a binary, so it has one of these. See prd.md FR-2.11.
    EngineInstaller *installer() const override { return m_installer; }

    // Where the engine is looked for, in the order it is tried:
    //
    //   1. $SQZ_ENGINE_EXE, for a development tree pointing at a copy
    //      elsewhere.
    //   2. engine\squeezelite.exe beside the application — where the installer
    //      puts it, and where win-deploy.bat stages it.
    //   3. engine\squeezelite.exe under the per-user data folder, which is
    //      where this app's own downloader falls back to when the application
    //      folder is not writable. An install elevated into Program Files is
    //      exactly that case, and it is not a case worth refusing to play in.
    //
    // Static and pure so the order is a test rather than a comment.
    static QStringList executableCandidates();
    static QString resolveExecutable(const QStringList &candidates);

    // Where a freshly downloaded engine should be written: candidate 2 when the
    // application folder can be written, candidate 3 when it cannot. Never
    // SQZ_ENGINE_EXE — an override that names a file someone already has is not
    // an answer to where a new one should go.
    static QString installDestination();

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

    // Notice an engine arriving or leaving. There is no event to subscribe to
    // here — the file can be put there by this app's downloader, by
    // fetch-engine.ps1, or by a user with an Explorer window — so this is
    // polled, like randomplayisactive and for the same reason. It is a
    // QFileInfo::exists on at most three paths, and it stops asking the moment
    // the answer is yes.
    void checkAvailability();

    QProcess *m_process = nullptr;
    QProcess *m_enumerator = nullptr;
    QTimer *m_restart = nullptr;
    QTimer *m_availability = nullptr;
    EngineInstaller *m_installer = nullptr;

    // Empty unless pinned by setExecutablePath(); the search runs otherwise.
    QString m_executableOverride;
    bool m_lastKnownAvailable = false;
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

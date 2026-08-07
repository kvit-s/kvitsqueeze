#pragma once

// Backend B (prd.md §7.3.2): supervise a stock squeezelite.exe child process.
//
// This is the only backend v1 builds. It ships the upstream binary unmodified
// and talks to it only through documented command-line arguments and its log
// output — which is what keeps SqeezeAmp's own licence unconstrained
// (prd.md §11.2). Do not patch the binary, and do not add a private channel
// to it.

#include "iaudioengine.h"

#include <QProcess>

class ExternalEngine : public IAudioEngine
{
    Q_OBJECT

public:
    explicit ExternalEngine(QObject *parent = nullptr);
    ~ExternalEngine() override;

    // Path to squeezelite.exe. Set before start(); defaults to "squeezelite"
    // beside the application, which is where the installer stages it.
    void setExecutablePath(const QString &path);

    bool start(const EngineConfig &config) override;
    void stop() override;
    bool setOutputDevice(const QString &device) override;
    QList<AudioDevice> devices() const override;
    EngineStatus status() const override { return m_status; }

    // The argument vector for a config, as a pure function so it can be
    // tested without launching anything. This is the entire interface to the
    // child process, so it is the thing worth pinning down in tests.
    //
    // Exclusive mode is `-a <latency>:<exclusive>` and is reachable from the
    // command line — the risk that would have ruled this backend out, and
    // does not (prd.md §7.3.2).
    static QStringList buildArguments(const EngineConfig &config);

private:
    void handleStandardError();
    void setState(EngineStatus::State state, const QString &error = {});

    QProcess *m_process = nullptr;
    QString m_executable;
    EngineConfig m_config;
    EngineStatus m_status;
};

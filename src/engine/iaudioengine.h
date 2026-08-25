// SPDX-License-Identifier: MPL-2.0

#pragma once

// The audio engine seam (prd.md §7.3).
//
// v1 has exactly one implementation, ExternalEngine, which supervises a stock
// squeezelite.exe child process. Two others are specified and deliberately
// not built — an in-process fork, and an own-SlimProto client — and this
// interface is the whole reason that decision stays reversible. Keep it
// honest: no ExternalEngine-specific type may appear above this header, and
// nothing here may name QProcess, a command-line flag or a log format.
//
// The engine knows nothing about LMS beyond an address to connect to. It is
// handed a config, told to start, and reports status back.

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class EngineInstaller;

// prd.md FR-2.8. A quality *preset* rather than a filter recipe, because the
// recipe grammar is one backend's command line and this header may not name
// one. Off is the default: the Windows mixer already resamples in shared mode
// and doing it twice is worse than doing it once (prd.md FR-2.4).
enum class ResampleQuality {
    Off,
    Fast,
    Balanced,
    High,
    VeryHigh,
};

struct EngineConfig
{
    QString serverHost;
    quint16 serverPort = 3483;   // SlimProto, not the 9000 control port

    // The player's identity on the server. Stable and persisted: the same id
    // across restarts keeps the queue and per-player settings server-side
    // (prd.md FR-1.4).
    //
    // Left empty by the caller in normal use — the engine fills it from
    // PlayerIdentity, because no module between here and the session is
    // allowed to carry a player id (prd.md FR-6.1). It stays a field so that
    // argument construction remains a pure function of its input.
    QString playerId;            // MAC-style, e.g. "aa:bb:cc:dd:ee:ff"
    QString playerName;

    QString outputDevice;        // empty = the engine's default
    int latencyMs = 0;           // 0 = the engine's default
    bool exclusive = false;      // prd.md FR-2.4 — P2, off by default
    ResampleQuality resample = ResampleQuality::Off;
};

// What the engine can say about itself. Fields it cannot determine stay at
// their unknown value and the UI hides them; reporting an unknown as 0 would
// have the diagnostics panel state a sample rate of zero as fact
// (prd.md FR-2.5).
struct EngineStatus
{
    enum class State {
        Stopped,     // not running, by request
        Starting,    // launched, not yet playing
        Running,
        Failed,      // died or refused to start; see lastError
    };

    State state = State::Stopped;
    QString lastError;

    QString decoder;             // empty = unknown
    QString outputDevice;        // what the engine actually opened
    int sourceSampleRate = -1;   // -1 = unknown
    int sourceBitDepth = -1;
    int outputSampleRate = -1;

    // 0.0-1.0, or -1.0 for unknown. Under ExternalEngine these are only ever
    // as good as what can be scraped from the child's log output, which is
    // the accepted cost of that backend (prd.md §7.3.4).
    double streamBufferFill = -1.0;
    double outputBufferFill = -1.0;
    int underruns = -1;
};

struct AudioDevice
{
    QString id;                  // what goes to the engine's device selector
    QString description;         // what the settings UI shows

    // Comparable so a re-enumeration that found the same devices does not
    // rebuild the settings combo box and lose the user's place in it.
    bool operator==(const AudioDevice &other) const
    {
        return id == other.id && description == other.description;
    }
};

class IAudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit IAudioEngine(QObject *parent = nullptr) : QObject(parent) {}
    ~IAudioEngine() override = default;

    // Can this backend run at all on this machine? Under Backend B that means
    // the binary is where it was staged; under an in-process backend it would
    // be trivially true. Asking lets the UI say "the audio engine is missing"
    // once, instead of failing at the first attempt to play.
    virtual bool isAvailable() const = 0;

    // Can this backend go and get whatever it needs, and how is that going?
    // Null when there is nothing to install — an in-process backend answers
    // nullptr and the UI shows no setup panel at all, which is why this is a
    // capability rather than a class the app is expected to know about.
    //
    // A backend that returns one must also start answering isAvailable() true
    // once it has run, without the app being restarted (prd.md FR-2.11).
    virtual EngineInstaller *installer() const { return nullptr; }

    virtual bool start(const EngineConfig &config) = 0;
    virtual void stop() = 0;

    // Changing the output device restarts the engine under ExternalEngine, so
    // the player briefly drops off the server and re-registers. The contract
    // is that this happens *gracefully* — no crash, no lost queue, no
    // duplicate player (prd.md FR-2.7).
    virtual bool setOutputDevice(const QString &device) = 0;

    virtual QList<AudioDevice> devices() const = 0;

    // Enumeration may cost a process launch or a driver round trip, so it is
    // an explicit request that answers on devicesChanged() rather than
    // something devices() does behind a const accessor.
    virtual void refreshDevices() = 0;

    virtual EngineStatus status() const = 0;

Q_SIGNALS:
    void statusChanged(const EngineStatus &status);
    void devicesChanged();
    void errorOccurred(const QString &message);

    // isAvailable() has changed its answer. Emitted when an engine appears or
    // disappears while the app is running — whether this app's own installer
    // put it there, the repair script did, or the user dropped it in by hand.
    // It exists so that "restart the app afterwards" is not an instruction
    // anyone has to be given (prd.md FR-2.11).
    void availabilityChanged();

    // Raw engine output, for the diagnostics panel (prd.md FR-9.2). Not a
    // parsing seam: anything the app needs to act on belongs in EngineStatus.
    void logLine(const QString &line);
};

// SPDX-License-Identifier: MPL-2.0

#pragma once

// The composition root: the one place the object graph is wired.
//
// Do not hand-build this graph anywhere else, tests included. A test that
// assembles its own session/engine/model set drifts from the real
// composition and then passes against a shape the app never has. If a test
// needs a different composition, it belongs in AppContext::Options.
//
// Everything QML can reach hangs off here as a property, so the surface the
// shell binds to is one list somebody can read. Note what is absent from that
// list, permanently: anything to do with another player (prd.md N5/FR-6.2),
// and any player id at all (FR-6.1).

// Full types rather than forward declarations: moc needs the complete type for
// a pointer Q_PROPERTY, and an incomplete one fails deep inside qmetatype.h
// with "Pointer Meta Types must either point to fully-defined types".
#include "artworkcache.h"
#include "diagnosticsmodel.h"
#include "enginecontroller.h"
#include "librarycontroller.h"
#include "lyricscontroller.h"
#include "playbackcontroller.h"
#include "queuemodel.h"
#include "randommixcontroller.h"
#include "searchmodel.h"
#include "settings.h"

#include <QObject>
#include <QVariantList>

class ExternalEngine;
class LmsSession;
class ServerBrowser;

class AppContext : public QObject
{
    Q_OBJECT

    Q_PROPERTY(Settings *settings READ settings CONSTANT)
    Q_PROPERTY(PlaybackController *player READ player CONSTANT)
    Q_PROPERTY(QueueModel *queue READ queue CONSTANT)
    Q_PROPERTY(LibraryController *library READ library CONSTANT)
    Q_PROPERTY(RandomMixController *mix READ mix CONSTANT)
    Q_PROPERTY(LyricsController *lyrics READ lyrics CONSTANT)
    Q_PROPERTY(SearchModel *search READ search CONSTANT)
    Q_PROPERTY(EngineController *engine READ engineController CONSTANT)
    Q_PROPERTY(DiagnosticsModel *diagnostics READ diagnostics CONSTANT)

    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionMessage READ connectionMessage NOTIFY connectionChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    // Where this build's source is. MPL-2.0 §3.2(a) obliges a distributed
    // binary to say so, and the About screen is where it does.
    Q_PROPERTY(QString sourceUrl READ sourceUrl CONSTANT)
    Q_PROPERTY(QString logDirectory READ logDirectory CONSTANT)

    // prd.md FR-1.1. Empty is a normal answer on a routed network — the
    // settings screen says so rather than looking broken.
    Q_PROPERTY(QVariantList discoveredServers READ discoveredServers NOTIFY discoveryChanged)
    Q_PROPERTY(bool scanning READ isScanning NOTIFY discoveryChanged)

public:
    struct Options
    {
        // Skip launching the audio engine. Used by the shell tests, which
        // exercise the UI against a session and have no business spawning a
        // real player onto the user's server.
        bool startEngine = true;

        // Skip connecting to the server, for the same reason.
        bool startSession = true;
    };

    explicit AppContext(const Options &options = {}, QObject *parent = nullptr);
    ~AppContext() override;

    LmsSession *session() const { return m_session; }
    Settings *settings() const { return m_settings; }
    PlaybackController *player() const { return m_player; }
    QueueModel *queue() const { return m_queue; }
    LibraryController *library() const { return m_library; }
    RandomMixController *mix() const { return m_mix; }
    LyricsController *lyrics() const { return m_lyrics; }
    SearchModel *search() const { return m_search; }
    EngineController *engineController() const { return m_engineController; }
    DiagnosticsModel *diagnostics() const { return m_diagnostics; }
    ArtworkCache *artwork() const { return m_artwork; }

    int connectionState() const;
    bool isConnected() const;
    QString connectionMessage() const { return m_connectionMessage; }
    QString version() const;
    QString sourceUrl() const;
    QString logDirectory() const;

    QVariantList discoveredServers() const { return m_discovered; }
    bool isScanning() const;

    // Everything the app does at startup that is not construction: read the
    // server out of settings, open the session, start the engine. Separate so
    // a test can build the graph without any of it happening.
    void begin();

    Q_INVOKABLE void scanForServers();
    Q_INVOKABLE void useServer(const QString &host, int port);

    // The `image://artwork/...` URL for a cover. Building it here keeps the
    // provider's scheme out of every QML file that shows a cover.
    Q_INVOKABLE QString artworkSource(const QString &coverId, int size) const;

    // Called once before the QML engine loads anything.
    static void registerQmlTypes();

Q_SIGNALS:
    void connectionChanged();
    void discoveryChanged();

private:
    void applyServerSettings();

    Options m_options;
    Settings *m_settings = nullptr;
    LmsSession *m_session = nullptr;
    ExternalEngine *m_engine = nullptr;
    EngineController *m_engineController = nullptr;
    PlaybackController *m_player = nullptr;
    QueueModel *m_queue = nullptr;
    LibraryController *m_library = nullptr;
    RandomMixController *m_mix = nullptr;
    LyricsController *m_lyrics = nullptr;
    SearchModel *m_search = nullptr;
    ArtworkCache *m_artwork = nullptr;
    DiagnosticsModel *m_diagnostics = nullptr;
    ServerBrowser *m_browser = nullptr;

    QVariantList m_discovered;
    QString m_connectionMessage;
};

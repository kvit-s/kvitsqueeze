#pragma once

// The library's entry points, the queue actions every browse row offers, and
// the server-scoped bits the settings screen shows.
//
// prd.md FR-3.1 fixes the top-level list: Artist, Album, Genre, Year,
// Playlist, Music Folder, New Music, Random Album. **That list is the complete
// set** — there is no plugin menu, no radio, no favourites and no generic
// renderer behind it (prd.md N4), so this class can enumerate the whole
// library surface in one header.
//
// FR-4.2 wants play now / play next / add to end from *every* browse context.
// That is one command, `playlistcontrol`, with whichever selector the row
// carries — so an album row, a genre row and a folder row all reach the queue
// through the same call and no screen needs its own queueing code.

#include "browsemodel.h"

#include <QObject>
#include <QString>
#include <QStringList>

class LmsSession;
class QTimer;

class LibraryController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString serverVersion READ serverVersion NOTIFY serverInfoChanged)
    Q_PROPERTY(int albumCount READ albumCount NOTIFY serverInfoChanged)
    Q_PROPERTY(int artistCount READ artistCount NOTIFY serverInfoChanged)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY serverInfoChanged)
    Q_PROPERTY(bool rescanning READ isRescanning NOTIFY rescanChanged)
    Q_PROPERTY(QString rescanDetail READ rescanDetail NOTIFY rescanChanged)

public:
    // Mirrors BrowseKind so QML has names for it without sqz-protocol needing
    // a QML registration of its own.
    enum Kind {
        Artists = int(BrowseKind::Artists),
        Albums = int(BrowseKind::Albums),
        Tracks = int(BrowseKind::Tracks),
        Genres = int(BrowseKind::Genres),
        Years = int(BrowseKind::Years),
        Playlists = int(BrowseKind::Playlists),
        PlaylistTracks = int(BrowseKind::PlaylistTracks),
        Folder = int(BrowseKind::Folder),
    };
    Q_ENUM(Kind)

    enum Action { PlayNow, PlayNext, AddToEnd };
    Q_ENUM(Action)

    explicit LibraryController(LmsSession *session, QObject *parent = nullptr);

    // A model for one browse screen. Returned without a parent, so the QML
    // engine takes ownership and collects it when the page that holds it is
    // popped — which is the only lifetime that matches a navigation stack.
    Q_INVOKABLE BrowseModel *browse(int kind, const QStringList &filters = {});

    // prd.md FR-3.1's two computed entry points. Neither is a new command:
    // "New Music" is albums sorted by when they were added, and "Random
    // Album" is the same list with a different sort — which is exactly why N4
    // could delete the generic menu renderer without losing anything.
    Q_INVOKABLE BrowseModel *newMusic();
    Q_INVOKABLE BrowseModel *randomAlbums();

    // ── prd.md FR-4.2, from every browse context.
    Q_INVOKABLE void enqueue(const QString &selectorKey, const QString &selectorValue,
                             int action);
    Q_INVOKABLE void enqueueFiltered(const QStringList &filters, int action);

    QString serverVersion() const { return m_serverVersion; }
    int albumCount() const { return m_albumCount; }
    int artistCount() const { return m_artistCount; }
    int trackCount() const { return m_trackCount; }
    bool isRescanning() const { return m_rescanning; }
    QString rescanDetail() const { return m_rescanDetail; }

    Q_INVOKABLE void refreshServerInfo();

    // prd.md FR-9.3 — triggering a rescan is in scope; everything else about
    // library management is the server's job (prd.md N3).
    Q_INVOKABLE void rescan(bool playlistsOnly);

Q_SIGNALS:
    void serverInfoChanged();
    void rescanChanged();

private:
    void pollRescan();

    LmsSession *m_session = nullptr;
    QTimer *m_rescanPoll = nullptr;

    QString m_serverVersion;
    int m_albumCount = 0;
    int m_artistCount = 0;
    int m_trackCount = 0;
    bool m_rescanning = false;
    QString m_rescanDetail;
};

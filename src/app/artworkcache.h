#pragma once

// Album art: fetched once, kept on disk, kept in memory, and never decoded on
// the GUI thread (prd.md FR-3.3, NFR-2, NFR-3).
//
// Three layers, each answering what the one below it would have to work for:
//
//   memory   QCache of decoded QImages, bounded in bytes by the setting
//            (default 64 MB). A grid that scrolls back over itself hits this.
//   disk     %LOCALAPPDATA%/SqeezeAmp/cache/artwork, bounded by the setting
//            (default 256 MB), swept on startup. Survives restarts, so the
//            second run of the app draws a browsed grid with no network at all.
//   server   one GET through LmsSession, because that is the only module
//            allowed to open a connection.
//
// Decoding happens on a QThreadPool. That is the part NFR-2 actually cares
// about: a 3000×3000 cover JPEG takes tens of milliseconds to decode, and a
// grid fling is fifty of them.
//
// Requests are coalesced by key. Twenty delegates asking for the same album
// cover during a fast scroll produce one fetch and twenty callbacks.

#include <QCache>
#include <QHash>
#include <QImage>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class LmsSession;
class Settings;

class ArtworkCache : public QObject
{
    Q_OBJECT

public:
    ArtworkCache(LmsSession *session, Settings *settings, QObject *parent = nullptr);

    // Delivered on the GUI thread, exactly once, with a null image when there
    // is no artwork. Must be called from the GUI thread.
    using Handler = std::function<void(const QImage &image)>;
    void request(const QString &coverId, int size, Handler handler);

    // Drop everything. Offered in settings for the case where the server's
    // artwork changed under a stable cover id.
    Q_INVOKABLE void clear();

    qint64 diskUsageBytes() const;

private:
    QString cacheKey(const QString &coverId, int size) const;
    QString filePath(const QString &coverId, int size) const;
    void deliver(const QString &key, const QImage &image);
    void decodeAsync(const QString &key, const QByteArray &data, int size, bool alsoWrite);
    void sweepDisk();

    LmsSession *m_session = nullptr;
    Settings *m_settings = nullptr;
    QString m_directory;

    QCache<QString, QImage> m_memory;
    QHash<QString, QList<Handler>> m_waiting;
};

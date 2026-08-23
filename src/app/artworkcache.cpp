// SPDX-License-Identifier: MPL-2.0

#include "artworkcache.h"

#include "lmssession.h"
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QStandardPaths>
#include <QThreadPool>
#include <QtConcurrent>

namespace {

// QCache costs are integers, so bytes would overflow the budget arithmetic on
// a large cache. Kilobytes keep a 64 MB budget at 65536 units.
int costInKilobytes(const QImage &image)
{
    return qMax(1, static_cast<int>(image.sizeInBytes() / 1024));
}

} // namespace

ArtworkCache::ArtworkCache(LmsSession *session, Settings *settings, QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_settings(settings)
{
    m_directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                  + QStringLiteral("/cache/artwork");
    QDir().mkpath(m_directory);

    m_memory.setMaxCost(qMax(1, m_settings->artworkMemoryCacheMb()) * 1024);

    // Once, at startup, off the GUI thread. Doing it on every write would turn
    // a scrolling grid into a directory walk per cover.
    QThreadPool::globalInstance()->start([this] { sweepDisk(); });
}

QString ArtworkCache::cacheKey(const QString &coverId, int size) const
{
    return coverId + QLatin1Char('@') + QString::number(size);
}

QString ArtworkCache::filePath(const QString &coverId, int size) const
{
    // The cover id is a server-side hex hash, so it is already a safe file
    // name. Sanitising anyway keeps a future non-hex id from escaping the
    // cache directory.
    QString safe = coverId;
    safe.removeIf([](QChar character) { return !character.isLetterOrNumber(); });
    if (safe.isEmpty())
        return {};
    return QStringLiteral("%1/%2_%3.jpg").arg(m_directory, safe, QString::number(size));
}

void ArtworkCache::request(const QString &coverId, int size, Handler handler)
{
    if (!handler)
        return;

    if (coverId.isEmpty() || size <= 0) {
        handler({});
        return;
    }

    const QString key = cacheKey(coverId, size);

    if (const QImage *cached = m_memory.object(key)) {
        handler(*cached);
        return;
    }

    // Coalesce: a fast scroll asks for the same cover from every delegate that
    // touches it, and one fetch answering all of them is the difference
    // between a grid that flies and one that saturates the link.
    const bool alreadyRunning = m_waiting.contains(key);
    m_waiting[key].append(std::move(handler));
    if (alreadyRunning)
        return;

    const QString path = filePath(coverId, size);
    const QUrl url = m_session->artworkUrl(coverId, size);

    QPointer<ArtworkCache> alive(this);

    // Try the disk first, on a worker. A hit means no network at all, which is
    // what makes the second run of the app draw a browsed grid instantly.
    (void)QtConcurrent::run([alive, this, key, path, url, size] {
        QByteArray onDisk;
        if (!path.isEmpty()) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly))
                onDisk = file.readAll();
        }

        if (!onDisk.isEmpty()) {
            QImage image;
            if (image.loadFromData(onDisk)) {
                QMetaObject::invokeMethod(this, [alive, this, key, image] {
                    if (alive)
                        deliver(key, image);
                }, Qt::QueuedConnection);
                return;
            }
            // A truncated file from an interrupted write. Falling through to
            // the network is better than caching a broken image forever.
            QFile::remove(path);
        }

        QMetaObject::invokeMethod(this, [alive, this, key, path, url, size] {
            if (!alive)
                return;
            if (!url.isValid()) {
                deliver(key, {});
                return;
            }
            m_session->get(url, [alive, this, key, path, size](const QByteArray &body, bool ok) {
                if (!alive)
                    return;
                if (!ok || body.isEmpty()) {
                    deliver(key, {});
                    return;
                }
                decodeAsync(key, body, size, !path.isEmpty());
            });
        }, Qt::QueuedConnection);
    });
}

void ArtworkCache::decodeAsync(const QString &key, const QByteArray &data, int size,
                               bool alsoWrite)
{
    const QString path = alsoWrite
                             ? filePath(key.section(QLatin1Char('@'), 0, 0), size)
                             : QString();
    QPointer<ArtworkCache> alive(this);

    (void)QtConcurrent::run([alive, this, key, data, path] {
        QImage image;
        image.loadFromData(data);

        if (!path.isEmpty() && !image.isNull()) {
            // Write to a temporary name and rename, so a crash mid-write
            // cannot leave a half-file that the next run reads as artwork.
            const QString temporary = path + QStringLiteral(".part");
            QFile file(temporary);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
                QFile::remove(path);
                QFile::rename(temporary, path);
            }
        }

        QMetaObject::invokeMethod(this, [alive, this, key, image] {
            if (alive)
                deliver(key, image);
        }, Qt::QueuedConnection);
    });
}

void ArtworkCache::deliver(const QString &key, const QImage &image)
{
    if (!image.isNull())
        m_memory.insert(key, new QImage(image), costInKilobytes(image));

    const QList<Handler> handlers = m_waiting.take(key);
    for (const Handler &handler : handlers)
        handler(image);
}

void ArtworkCache::clear()
{
    m_memory.clear();
    QDir(m_directory).removeRecursively();
    QDir().mkpath(m_directory);
}

qint64 ArtworkCache::diskUsageBytes() const
{
    qint64 total = 0;
    const QFileInfoList files = QDir(m_directory).entryInfoList(QDir::Files);
    for (const QFileInfo &file : files)
        total += file.size();
    return total;
}

void ArtworkCache::sweepDisk()
{
    const qint64 budget = qint64(qMax(1, m_settings->artworkDiskCacheMb())) * 1024 * 1024;

    QFileInfoList files = QDir(m_directory).entryInfoList(QDir::Files, QDir::Time);
    qint64 total = 0;
    for (const QFileInfo &file : files)
        total += file.size();

    if (total <= budget)
        return;

    // entryInfoList(QDir::Time) is newest first, so deleting from the back
    // evicts the least recently touched — an LRU by file time, which is what
    // prd.md NFR-3's "bounded and configurable" needs and no more.
    for (auto it = files.rbegin(); it != files.rend() && total > budget; ++it) {
        const qint64 size = it->size();
        if (QFile::remove(it->absoluteFilePath()))
            total -= size;
    }
}

#include "artworkimageprovider.h"

#include "artworkcache.h"

#include <QQuickTextureFactory>

QQuickTextureFactory *ArtworkImageResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void ArtworkImageResponse::deliver(const QImage &image)
{
    m_image = image;
    // finished() is connected by the QML pixmap reader with itself as the
    // context object, so emitting from the GUI thread queues into the reader's
    // thread rather than touching it directly.
    Q_EMIT finished();
}

ArtworkImageProvider::ArtworkImageProvider(ArtworkCache *cache)
    : QQuickAsyncImageProvider()
    , m_cache(cache)
{
}

QQuickImageResponse *ArtworkImageProvider::requestImageResponse(const QString &id,
                                                                const QSize &requestedSize)
{
    auto *response = new ArtworkImageResponse;

    // "<coverId>/<size>". The size is in the URL rather than taken from
    // requestedSize because it decides which file the server renders and
    // therefore which cache entry is being asked for — sourceSize on the QML
    // side would change the decode, not the fetch.
    const QString coverId = id.section(QLatin1Char('/'), 0, 0);
    int size = id.section(QLatin1Char('/'), 1, 1).toInt();
    if (size <= 0)
        size = qMax(requestedSize.width(), requestedSize.height());
    if (size <= 0)
        size = 300;

    if (!m_cache || coverId.isEmpty()) {
        // Queued, not immediate. The QML pixmap reader connects to finished()
        // *after* this function returns, so a response that has already
        // finished is a response whose signal is never seen — and an Image
        // that never settles keeps its placeholder forever.
        QMetaObject::invokeMethod(response, [response] { response->deliver({}); },
                                  Qt::QueuedConnection);
        return response;
    }

    QPointer<ArtworkImageResponse> alive(response);
    ArtworkCache *cache = m_cache;

    // This runs on the QML image thread; the cache is a GUI-thread object.
    QMetaObject::invokeMethod(cache, [cache, alive, coverId, size] {
        if (!alive)
            return;
        cache->request(coverId, size, [alive](const QImage &image) {
            // The engine deletes a response after taking its texture, and
            // cancels outright when the Image element goes away mid-scroll.
            if (alive)
                alive->deliver(image);
        });
    }, Qt::QueuedConnection);

    return response;
}

#pragma once

// The bridge between QML's `image://artwork/<coverId>/<size>` and ArtworkCache.
//
// Async rather than the plain provider: requestImage() would be called on the
// QML image thread and would have to block there, which turns a fast scroll
// over uncached covers into a stalled image pipeline. This returns immediately
// and the delegate draws its placeholder until the image arrives
// (prd.md FR-3.3, NFR-2).
//
// The cache itself lives on the GUI thread, because a miss goes out through
// LmsSession and that machinery is bound to the thread it was created on. So
// every call into it is marshalled. Decoding still happens on a pool thread
// inside the cache — the GUI thread only ever hands over a request and
// receives a finished QImage.

#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QImage>
#include <QPointer>

class ArtworkCache;

class ArtworkImageResponse : public QQuickImageResponse
{
    Q_OBJECT

public:
    QQuickTextureFactory *textureFactory() const override;

    // Called on the GUI thread when the cache has an answer — including a null
    // image, which is a legitimate answer meaning "this track has no artwork".
    void deliver(const QImage &image);

private:
    QImage m_image;
};

class ArtworkImageProvider : public QQuickAsyncImageProvider
{
public:
    explicit ArtworkImageProvider(ArtworkCache *cache);

    QQuickImageResponse *requestImageResponse(const QString &id,
                                              const QSize &requestedSize) override;

private:
    QPointer<ArtworkCache> m_cache;
};

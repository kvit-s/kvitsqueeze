// SPDX-License-Identifier: MPL-2.0

#include "lyricssidecar.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

QStringList LyricsSidecar::candidates(const QString &trackUrl, const QString &localRoot)
{
    const QString root = localRoot.trimmed();
    if (root.isEmpty() || trackUrl.isEmpty())
        return {};

    const QUrl url(trackUrl);

    // A stream is not a file and has no sidecar. LMS reports one as http://,
    // and prd.md FR-2.5's rule applies: no guess is better than a wrong one.
    if (!url.isLocalFile() && !url.scheme().isEmpty() && url.scheme() != QLatin1String("file"))
        return {};

    // The server's path, decoded — `%20` and `%23` are how a space and a `#`
    // reach us, and a filename containing either is ordinary.
    QString path = url.path();
    if (path.isEmpty())
        path = trackUrl;
    path = QDir::fromNativeSeparators(path);

    // Windows file URLs arrive as /C:/music/…; the leading slash is not part
    // of the path.
    if (path.size() > 2 && path.at(0) == QLatin1Char('/') && path.at(2) == QLatin1Char(':'))
        path.remove(0, 1);

    const QFileInfo info(path);
    if (info.fileName().isEmpty())
        return {};

    // The sidecar shares the track's basename: every one of the 1109 in the
    // library this was built against does.
    QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return {};
    parts.last() = info.completeBaseName() + QStringLiteral(".lrc");

    const QString base = QDir::fromNativeSeparators(root);

    QStringList out;
    out.reserve(parts.size());
    for (int first = 0; first < parts.size(); ++first) {
        const QString tail = QStringList(parts.mid(first)).join(QLatin1Char('/'));
        out.append(QDir::cleanPath(base + QLatin1Char('/') + tail));
    }
    return out;
}

#include "browsemodel.h"

#include "lmscommands.h"
#include "lmssession.h"

#include <QPointer>
#include <QVariantMap>

namespace {

// Big enough that a fast scroll crosses few boundaries, small enough that the
// first screenful arrives immediately. A `titles` page at this size is around
// 30 KB on the wire.
constexpr int kPageSize = 120;

QStringList commandFor(BrowseKind kind, int start, int count, const QStringList &filters)
{
    switch (kind) {
    case BrowseKind::Artists:
        return LmsCommand::artists(start, count, filters + QStringList{ QStringLiteral("tags:s") });
    case BrowseKind::Albums:
        return LmsCommand::albums(start, count, filters + QStringList{ LmsCommand::albumTags() });
    case BrowseKind::Tracks:
        return LmsCommand::titles(start, count, filters + QStringList{ LmsCommand::trackTags() });
    case BrowseKind::Genres:
        return LmsCommand::genres(start, count, filters);
    case BrowseKind::Years:
        return LmsCommand::years(start, count, filters);
    case BrowseKind::Playlists:
        return LmsCommand::playlists(start, count);
    case BrowseKind::PlaylistTracks: {
        // The playlist id travels as an ordinary filter so that every screen
        // is configured the same way; the command needs it as an argument.
        QString playlistId;
        for (const QString &filter : filters) {
            if (filter.startsWith(QLatin1String("playlist_id:")))
                playlistId = filter.section(QLatin1Char(':'), 1);
        }
        return LmsCommand::playlistTracks(start, count, playlistId);
    }
    case BrowseKind::Folder: {
        QString folderId;
        for (const QString &filter : filters) {
            if (filter.startsWith(QLatin1String("folder_id:")))
                folderId = filter.section(QLatin1Char(':'), 1);
        }
        return LmsCommand::musicFolder(start, count, folderId);
    }
    }
    return {};
}

} // namespace

BrowseModel::BrowseModel(LmsSession *session, BrowseKind kind, const QStringList &filters,
                         QObject *parent)
    : QAbstractListModel(parent)
    , m_session(session)
    , m_kind(kind)
    , m_filters(filters)
{
    // A screen opened while the server is unreachable — during a reconnect, or
    // in the moment before the session has one — would otherwise sit on
    // "Loading…" until the user navigated away and back. prd.md NFR-7 wants a
    // reconnect to be survivable without restarting the app, and an empty list
    // that never fills is the most visible way to fail that.
    connect(m_session, &LmsSession::stateChanged, this, [this](LmsSession::State state) {
        if (state == LmsSession::State::Connected && !m_firstPageDone)
            fetchPage(0);
    });

    fetchPage(0);
}

int BrowseModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_total;
}

int BrowseModel::pageOf(int row) const
{
    return row / kPageSize;
}

QVariant BrowseModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_total)
        return {};

    const auto found = m_rows.constFind(index.row());
    if (found == m_rows.constEnd()) {
        // A row whose page has not arrived. Answering with empties rather than
        // blocking is what makes the list scroll at full speed over unfetched
        // regions; the delegate draws a placeholder because LoadedRole is
        // false, and calling ensureLoaded() is its job, not this accessor's —
        // data() is called during painting and must not start I/O.
        if (role == LoadedRole)
            return false;
        if (role == DurationRole)
            return -1.0;
        if (role == YearRole || role == TrackNumberRole)
            return -1;
        if (role == IsFolderRole)
            return false;
        return QString();
    }

    const BrowseItem &item = *found;
    switch (static_cast<Roles>(role)) {
    case ItemIdRole:      return item.id;
    case TitleRole:       return item.title;
    case SubtitleRole:    return item.subtitle;
    case CoverIdRole:     return item.coverId;
    case DurationRole:    return item.duration;
    case YearRole:        return item.year;
    case TrackNumberRole: return item.trackNumber;
    case TextKeyRole:     return item.textKey;
    case AlbumIdRole:     return item.albumId;
    case ArtistIdRole:    return item.artistId;
    case IsFolderRole:    return item.isFolder;
    case LoadedRole:      return true;
    }
    return {};
}

QHash<int, QByteArray> BrowseModel::roleNames() const
{
    return {
        { ItemIdRole,      "itemId" },
        { TitleRole,       "title" },
        { SubtitleRole,    "subtitle" },
        { CoverIdRole,     "coverId" },
        { DurationRole,    "duration" },
        { YearRole,        "year" },
        { TrackNumberRole, "trackNumber" },
        { TextKeyRole,     "textKey" },
        { AlbumIdRole,     "albumId" },
        { ArtistIdRole,    "artistId" },
        { IsFolderRole,    "isFolder" },
        { LoadedRole,      "loaded" },
    };
}

void BrowseModel::ensureLoaded(int row)
{
    if (row < 0 || row >= m_total)
        return;
    fetchPage(pageOf(row));
}

void BrowseModel::reload()
{
    beginResetModel();
    m_rows.clear();
    m_havePage.clear();
    m_inFlight.clear();
    m_total = 0;
    m_firstPageDone = false;
    endResetModel();
    Q_EMIT totalChanged();
    fetchPage(0);
}

void BrowseModel::fetchPage(int page)
{
    if (page < 0 || m_havePage.contains(page) || m_inFlight.contains(page))
        return;

    m_inFlight.insert(page);
    if (m_inFlight.size() == 1)
        Q_EMIT loadingChanged();

    const int start = page * kPageSize;

    // The reply outlives the model whenever a browse page is popped while its
    // fetch is in flight — which is exactly what a fast back-button does. The
    // handler is owned by the session, so without the guard it would run
    // against a deleted model.
    QPointer<BrowseModel> alive(this);

    m_session->send(commandFor(m_kind, start, kPageSize, m_filters),
                    [this, alive, page, start](const QJsonObject &result) {
                        if (!alive)
                            return;
                        m_inFlight.remove(page);
                        if (m_inFlight.isEmpty())
                            Q_EMIT loadingChanged();

                        if (result.isEmpty()) {
                            Q_EMIT error(tr("The server did not answer the browse request"));
                            return;
                        }

                        const BrowseReply reply =
                            BrowseReply::fromResult(result, m_kind, start);

                        if (reply.total != m_total) {
                            // The row count changing is a structural change,
                            // so it goes through begin/endResetModel rather
                            // than a dataChanged the view would ignore.
                            beginResetModel();
                            m_total = reply.total;
                            // Rows past the new total are gone; rows before it
                            // are still valid, and dropping them would refetch
                            // the whole list every time the count moves.
                            for (auto it = m_rows.begin(); it != m_rows.end();) {
                                if (it.key() >= m_total)
                                    it = m_rows.erase(it);
                                else
                                    ++it;
                            }
                            endResetModel();
                            Q_EMIT totalChanged();
                        }

                        m_havePage.insert(page);

                        int row = start;
                        for (const BrowseItem &item : reply.items) {
                            if (row < m_total)
                                m_rows.insert(row, item);
                            ++row;
                        }

                        if (!reply.items.isEmpty()) {
                            const QModelIndex from = index(start);
                            const QModelIndex to = index(qMin(row, m_total) - 1);
                            if (from.isValid() && to.isValid())
                                Q_EMIT dataChanged(from, to);
                        }

                        if (!m_firstPageDone) {
                            m_firstPageDone = true;
                            Q_EMIT totalChanged();
                        }
                    });
}

QVariantMap BrowseModel::get(int row) const
{
    QVariantMap map;
    const QModelIndex modelIndex = index(row);
    if (!modelIndex.isValid())
        return map;

    const QHash<int, QByteArray> names = roleNames();
    for (auto it = names.constBegin(); it != names.constEnd(); ++it)
        map.insert(QString::fromUtf8(it.value()), data(modelIndex, it.key()));
    return map;
}

QString BrowseModel::selectorKey() const
{
    return BrowseFilters::selectorKey(m_kind);
}

QString BrowseModel::selectorValue(int row) const
{
    return m_rows.value(row).id;
}

QStringList BrowseModel::childFilters(int row) const
{
    // The rule itself is a pure function in sqz-protocol, where it can be
    // tested without a server (prd.md FR-3.4).
    return BrowseFilters::accumulate(m_kind, m_filters, selectorValue(row));
}

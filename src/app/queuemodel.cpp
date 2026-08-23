// SPDX-License-Identifier: MPL-2.0

#include "queuemodel.h"

#include "lmscommands.h"
#include "lmssession.h"

#include <QVariantMap>

QueueModel::QueueModel(LmsSession *session, QObject *parent)
    : QAbstractListModel(parent)
    , m_session(session)
{
}

int QueueModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_entries.size());
}

QVariant QueueModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const QueueTrack &entry = m_entries.at(index.row());
    switch (static_cast<Roles>(role)) {
    case TitleRole:     return entry.title;
    case ArtistRole:    return entry.artist;
    case AlbumRole:     return entry.album;
    case DurationRole:  return entry.duration;
    case CoverIdRole:   return entry.coverId;
    case TrackIdRole:   return entry.id;
    case AlbumIdRole:   return entry.albumId;
    case IsCurrentRole: return index.row() == m_currentIndex;
    }
    return {};
}

QHash<int, QByteArray> QueueModel::roleNames() const
{
    return {
        { TitleRole,     "title" },
        { ArtistRole,    "artist" },
        { AlbumRole,     "album" },
        { DurationRole,  "duration" },
        { CoverIdRole,   "coverId" },
        { TrackIdRole,   "trackId" },
        { AlbumIdRole,   "albumId" },
        { IsCurrentRole, "isCurrent" },
    };
}

QVariantMap QueueModel::get(int row) const
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

double QueueModel::totalDuration() const
{
    double total = 0;
    for (const QueueTrack &entry : m_entries) {
        if (entry.duration > 0)
            total += entry.duration;
    }
    return total;
}

void QueueModel::applySnapshot(const PlayerStatus &status)
{
    if (!status.valid)
        return;

    // A window that does not start at the top is a now-playing snapshot, not
    // the queue. Applying it would collapse the whole queue to one row.
    if (status.queueIncluded && status.queueStart != 0)
        return;

    m_serverCount = status.playlistCount;

    if (status.playlistCount == 0) {
        if (!m_entries.isEmpty()) {
            beginResetModel();
            m_entries.clear();
            endResetModel();
            Q_EMIT countChanged();
        }
        m_playlistTimestamp = status.playlistTimestamp;
        setCurrentIndex(-1);
        return;
    }

    // A full reset is right here even though it costs the view's scroll
    // position: the server sends contents and order together with no diff, and
    // guessing a diff from two flat lists would reorder rows the server did
    // not touch. Contents change rarely — the cursor, which changes on every
    // track, goes through applyCursor() and touches one row.
    beginResetModel();
    m_entries = status.queue;
    endResetModel();

    m_playlistTimestamp = status.playlistTimestamp;
    Q_EMIT countChanged();
    setCurrentIndex(status.playlistIndex);
}

void QueueModel::applyCursor(const PlayerStatus &status)
{
    if (!status.valid)
        return;

    m_serverCount = status.playlistCount;
    setCurrentIndex(status.playlistIndex);
}

void QueueModel::setCurrentIndex(int index)
{
    if (index == m_currentIndex)
        return;

    const int previous = m_currentIndex;
    m_currentIndex = index;

    // Repaint only the two rows whose highlight actually moved.
    const auto touch = [this](int row) {
        if (row >= 0 && row < m_entries.size()) {
            const QModelIndex modelIndex = createIndex(row, 0);
            Q_EMIT dataChanged(modelIndex, modelIndex, { IsCurrentRole });
        }
    };
    touch(previous);
    touch(m_currentIndex);

    Q_EMIT currentIndexChanged();
}

void QueueModel::playIndex(int index)
{
    if (index < 0)
        return;
    m_session->send(LmsCommand::playlistJumpTo(index));
}

void QueueModel::removeIndex(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;
    m_session->send(LmsCommand::playlistRemove(index));
    m_session->refreshQueue();
}

void QueueModel::move(int from, int to)
{
    if (from == to || from < 0 || to < 0
        || from >= m_entries.size() || to >= m_entries.size())
        return;
    m_session->send(LmsCommand::playlistMove(from, to));
    m_session->refreshQueue();
}

void QueueModel::clear()
{
    m_session->send(LmsCommand::playlistClear());
    m_session->refreshQueue();
}

void QueueModel::saveAs(const QString &name)
{
    if (name.trimmed().isEmpty())
        return;
    m_session->send(LmsCommand::playlistSaveAs(name.trimmed()));
}

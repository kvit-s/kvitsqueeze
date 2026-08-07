#include "queuemodel.h"

QueueModel::QueueModel(QObject *parent)
    : QAbstractListModel(parent)
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

    const Entry &entry = m_entries.at(index.row());
    switch (static_cast<Roles>(role)) {
    case TitleRole:    return entry.title;
    case ArtistRole:   return entry.artist;
    case AlbumRole:    return entry.album;
    case DurationRole: return entry.duration;
    case CoverIdRole:  return entry.coverId;
    }
    return {};
}

QHash<int, QByteArray> QueueModel::roleNames() const
{
    return {
        { TitleRole,    "title" },
        { ArtistRole,   "artist" },
        { AlbumRole,    "album" },
        { DurationRole, "duration" },
        { CoverIdRole,  "coverId" },
    };
}

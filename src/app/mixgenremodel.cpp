// SPDX-License-Identifier: MPL-2.0

#include "mixgenremodel.h"

MixGenreModel::MixGenreModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MixGenreModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return count();
}

QVariant MixGenreModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_genres.size())
        return {};

    const RandomMix::Genre &genre = m_genres.at(index.row());
    switch (static_cast<Roles>(role)) {
    case NameRole:     return genre.name;
    case IncludedRole: return genre.included;
    }
    return {};
}

QHash<int, QByteArray> MixGenreModel::roleNames() const
{
    return {
        { NameRole,     "name" },
        { IncludedRole, "included" },
    };
}

QVariantMap MixGenreModel::get(int row) const
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

int MixGenreModel::includedCount() const
{
    int included = 0;
    for (const RandomMix::Genre &genre : m_genres) {
        if (genre.included)
            ++included;
    }
    return included;
}

bool MixGenreModel::sameNames(const QList<RandomMix::Genre> &genres) const
{
    if (genres.size() != m_genres.size())
        return false;
    for (int row = 0; row < genres.size(); ++row) {
        if (genres.at(row).name != m_genres.at(row).name)
            return false;
    }
    return true;
}

void MixGenreModel::replace(const QList<RandomMix::Genre> &genres)
{
    const bool wasLoaded = m_loaded;
    m_loaded = true;

    if (wasLoaded && sameNames(genres)) {
        bool anyChanged = false;
        for (int row = 0; row < genres.size(); ++row) {
            if (m_genres.at(row).included == genres.at(row).included)
                continue;
            m_genres[row].included = genres.at(row).included;
            const QModelIndex modelIndex = createIndex(row, 0);
            Q_EMIT dataChanged(modelIndex, modelIndex, { IncludedRole });
            anyChanged = true;
        }
        if (anyChanged)
            Q_EMIT includedChanged();
        return;
    }

    beginResetModel();
    m_genres = genres;
    endResetModel();

    Q_EMIT countChanged();
    Q_EMIT includedChanged();
}

void MixGenreModel::setIncluded(const QString &name, bool included)
{
    for (int row = 0; row < m_genres.size(); ++row) {
        if (m_genres.at(row).name != name)
            continue;
        if (m_genres.at(row).included == included)
            return;

        m_genres[row].included = included;
        const QModelIndex modelIndex = createIndex(row, 0);
        Q_EMIT dataChanged(modelIndex, modelIndex, { IncludedRole });
        Q_EMIT includedChanged();
        return;
    }
}

void MixGenreModel::setAllIncluded(bool included)
{
    if (m_genres.isEmpty())
        return;

    bool anyChanged = false;
    for (int row = 0; row < m_genres.size(); ++row) {
        if (m_genres.at(row).included == included)
            continue;
        m_genres[row].included = included;
        anyChanged = true;
    }
    if (!anyChanged)
        return;

    const QModelIndex first = createIndex(0, 0);
    const QModelIndex last = createIndex(static_cast<int>(m_genres.size()) - 1, 0);
    Q_EMIT dataChanged(first, last, { IncludedRole });
    Q_EMIT includedChanged();
}

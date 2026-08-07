#pragma once

// The play queue, as QML sees it.
//
// Skeleton: roles and the shape are fixed, the fill comes from the
// `status`/`playlist` replies at M2 (prd.md FR-4.1).

#include <QAbstractListModel>
#include <QList>
#include <QString>

class QueueModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        ArtistRole,
        AlbumRole,
        DurationRole,
        CoverIdRole,
    };

    explicit QueueModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int currentIndex() const { return m_currentIndex; }

Q_SIGNALS:
    void currentIndexChanged();

private:
    struct Entry
    {
        QString title;
        QString artist;
        QString album;
        QString coverId;
        double duration = -1.0;
    };

    QList<Entry> m_entries;
    int m_currentIndex = -1;
};

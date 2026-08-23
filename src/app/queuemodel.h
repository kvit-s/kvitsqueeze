// SPDX-License-Identifier: MPL-2.0

#pragma once

// The play queue, as QML sees it (prd.md §8.4).
//
// The queue is the server's, not ours. Every edit is a command; nothing is
// reordered locally and then pushed. What arrives back on the next snapshot is
// what the queue is — including when the change came from somewhere else
// entirely (prd.md FR-6.4).
//
// The one exception is a drag: a row follows the pointer locally because it
// has to, and the model is rebuilt from the server's answer when the drop
// lands. A drop that the server rejects therefore snaps back, which is the
// correct outcome and is visible rather than silent.

#include "playerstatus.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>

class LmsSession;

class QueueModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool truncated READ isTruncated NOTIFY countChanged)
    Q_PROPERTY(double totalDuration READ totalDuration NOTIFY countChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        ArtistRole,
        AlbumRole,
        DurationRole,
        CoverIdRole,
        TrackIdRole,
        AlbumIdRole,
        IsCurrentRole,
    };
    Q_ENUM(Roles)

    explicit QueueModel(LmsSession *session, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int currentIndex() const { return m_currentIndex; }

    // True when the server's queue is longer than the window that was
    // fetched. The view says so rather than letting the user believe a
    // 3000-track queue ends at row 2000.
    bool isTruncated() const { return m_serverCount > m_entries.size(); }
    double totalDuration() const;

    // ── Edits (prd.md FR-4.3). Each one is a command; the model changes when
    // the server says it did.
    Q_INVOKABLE void playIndex(int index);
    Q_INVOKABLE void removeIndex(int index);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void saveAs(const QString &name);

    // One row as a plain object, keyed by role name — for the places QML needs
    // a row outside a delegate, such as the "up next" strip. The alternative
    // is arithmetic on Qt.UserRole in QML, which breaks quietly the first time
    // a role is inserted in the middle of the enum.
    Q_INVOKABLE QVariantMap get(int row) const;

public Q_SLOTS:
    // Fed by LmsSession::queueReceived — a snapshot that carried a window
    // starting at the top of the queue. A plain status may carry one track and
    // must never be mistaken for the whole thing.
    void applySnapshot(const PlayerStatus &status);

    // Fed by every status: the cursor moves far more often than the contents
    // do, and repainting one row beats rebuilding the model.
    void applyCursor(const PlayerStatus &status);

Q_SIGNALS:
    void currentIndexChanged();
    void countChanged();

private:
    void setCurrentIndex(int index);

    LmsSession *m_session = nullptr;
    QList<QueueTrack> m_entries;
    int m_currentIndex = -1;
    int m_serverCount = 0;
    double m_playlistTimestamp = 0.0;
};

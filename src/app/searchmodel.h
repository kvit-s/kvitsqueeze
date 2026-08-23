// SPDX-License-Identifier: MPL-2.0

#pragma once

// Global search across artists, albums and tracks (prd.md FR-3.5).
//
// One flat model with a section role rather than three models, because that is
// what a sectioned ListView wants and because the three sections are always
// drawn together (prd.md §9.2). The section is a role, so the view groups
// them without this class knowing anything about how they are drawn.
//
// Queries are debounced. Typing "beatles" is eight keystrokes and would be
// eight `search` commands against a 50k-track library without it; the
// in-flight generation counter is what keeps a slow early reply from
// overwriting a fast late one.

#include "browsereply.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>

class LmsSession;
class QTimer;

class SearchModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QString term READ term WRITE setTerm NOTIFY termChanged)
    Q_PROPERTY(bool searching READ isSearching NOTIFY searchingChanged)
    Q_PROPERTY(bool hasResults READ hasResults NOTIFY resultsChanged)
    Q_PROPERTY(int artistTotal READ artistTotal NOTIFY resultsChanged)
    Q_PROPERTY(int albumTotal READ albumTotal NOTIFY resultsChanged)
    Q_PROPERTY(int trackTotal READ trackTotal NOTIFY resultsChanged)

public:
    enum Section { ArtistSection, AlbumSection, TrackSection };
    Q_ENUM(Section)

    enum Roles {
        ItemIdRole = Qt::UserRole + 1,
        TitleRole,
        SubtitleRole,
        SectionRole,       // the Section enum, for grouping
        SectionNameRole,   // the translated heading
        SelectorKeyRole,   // "artist_id" / "album_id" / "track_id"
    };
    Q_ENUM(Roles)

    explicit SearchModel(LmsSession *session, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString term() const { return m_term; }
    void setTerm(const QString &term);

    bool isSearching() const { return m_pending > 0; }
    bool hasResults() const { return !m_rows.isEmpty(); }
    int artistTotal() const { return m_reply.artistTotal; }
    int albumTotal() const { return m_reply.albumTotal; }
    int trackTotal() const { return m_reply.trackTotal; }

    Q_INVOKABLE void clear();

Q_SIGNALS:
    void termChanged();
    void searchingChanged();
    void resultsChanged();

private:
    struct Row
    {
        BrowseItem item;
        Section section;
    };

    void run();
    void rebuild();

    LmsSession *m_session = nullptr;
    QTimer *m_debounce = nullptr;
    QString m_term;
    SearchReply m_reply;
    QList<Row> m_rows;

    // Bumped for every request sent and captured by its handler, so a reply
    // that arrives after a newer one was issued is discarded instead of
    // repainting the list with stale results.
    quint64 m_generation = 0;
    int m_pending = 0;
};

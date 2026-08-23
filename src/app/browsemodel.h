// SPDX-License-Identifier: MPL-2.0

#pragma once

// One list model for every browse screen (prd.md FR-3.2, FR-3.4).
//
// **Virtualized against the server, not just the view.** The model's row count
// is the server's total for the filtered set as soon as the first window comes
// back, so a 50k-track list is 50k rows immediately and the scrollbar is the
// right size. Rows the app has not fetched answer with empty strings, and the
// delegate asking for one is what schedules its page. Nothing blocks and
// nothing is fetched twice.
//
// **Composable filters, one model.** Genre → Artist → Album is not three
// screens; it is this model with `["genre_id:405"]`, then
// `["genre_id:405", "artist_id:6759"]`. Any reachable combination works
// because the filters are just accumulated params on the same typed command
// (verified against Lyrion Music Server 9.1.0 — prd.md §14 assumption 3).
//
// Nothing here knows about menu descriptors, plugins or `browselibrary`
// (prd.md N4). A row is a BrowseItem and a screen is a kind plus a filter set.

#include "browsereply.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

class LmsSession;
class QTimer;

class BrowseModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int total READ total NOTIFY totalChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY totalChanged)
    Q_PROPERTY(int kind READ kindValue CONSTANT)

public:
    enum Roles {
        ItemIdRole = Qt::UserRole + 1,
        TitleRole,
        SubtitleRole,
        CoverIdRole,
        DurationRole,
        YearRole,
        TrackNumberRole,
        TextKeyRole,
        AlbumIdRole,
        ArtistIdRole,
        IsFolderRole,
        LoadedRole,
    };
    Q_ENUM(Roles)

    BrowseModel(LmsSession *session, BrowseKind kind, const QStringList &filters,
                QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int total() const { return m_total; }
    bool isLoading() const { return !m_inFlight.isEmpty(); }
    bool isLoaded() const { return m_firstPageDone; }
    int kindValue() const { return static_cast<int>(m_kind); }
    BrowseKind kind() const { return m_kind; }
    QStringList filters() const { return m_filters; }

    // Called by the delegate for the row it is about to show. Cheap and
    // idempotent: it returns immediately unless the row's page is missing and
    // not already being fetched.
    Q_INVOKABLE void ensureLoaded(int row);

    // Throw away every fetched page and start again — for a rescan, or a
    // reconnect that may have changed what the server holds.
    Q_INVOKABLE void reload();

    // The selector this row contributes to a queue command or a drill-down,
    // e.g. {"album_id", "5254"}. Empty for a row that is not yet loaded.
    Q_INVOKABLE QString selectorKey() const;
    Q_INVOKABLE QString selectorValue(int row) const;

    // The filter set a child screen should carry: this model's filters plus
    // this row's own. That accumulation *is* FR-3.4.
    Q_INVOKABLE QStringList childFilters(int row) const;

    // A row as a plain object, keyed by role name. QML needs a row's fields
    // outside a delegate — a keyboard Return has no delegate in hand — and the
    // alternative is arithmetic on Qt.UserRole in QML, which breaks silently
    // the first time a role is inserted in the middle of the enum.
    Q_INVOKABLE QVariantMap get(int row) const;

Q_SIGNALS:
    void totalChanged();
    void loadingChanged();
    void error(const QString &message);

private:
    void fetchPage(int page);
    int pageOf(int row) const;

    LmsSession *m_session = nullptr;
    BrowseKind m_kind;
    QStringList m_filters;

    // Sparse storage: only fetched rows exist. A QHash rather than a
    // pre-sized list because a 50k-row list of empty structs is 5 MB of
    // nothing, and prd.md NFR-3 budgets 200 MB for the whole app.
    QHash<int, BrowseItem> m_rows;
    QSet<int> m_inFlight;
    QSet<int> m_havePage;

    int m_total = 0;
    bool m_firstPageDone = false;
};

#pragma once

// The genres a random mix is allowed to draw from (prd.md FR-3.9).
//
// This is a *server-side pref*, not player state: it is shared by every mix,
// it survives a restart, and changing it here changes it for the LMS web UI
// too. That is why the scope is shown beside the mix control rather than
// buried in a dialog — a narrowed scope forgotten three weeks ago is otherwise
// indistinguishable from a library that has gone strange.
//
// Rows arrive already reduced to a name and a flag; see
// RandomMix::genresFromListResult() for why reading that reply is an
// extraction rather than the menu rendering prd.md N4 rules out.

#include "randommix.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariantMap>

class MixGenreModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int includedCount READ includedCount NOTIFY includedChanged)
    Q_PROPERTY(bool narrowed READ isNarrowed NOTIFY includedChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        IncludedRole,
    };

    explicit MixGenreModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Reading a row outside a delegate. See CLAUDE.md: arithmetic on
    // Qt::UserRole in QML breaks the first time a role is inserted.
    Q_INVOKABLE QVariantMap get(int row) const;

    int count() const { return static_cast<int>(m_genres.size()); }
    int includedCount() const;
    bool isNarrowed() const { return includedCount() < count(); }
    bool isLoaded() const { return m_loaded; }

    // Take a fresh list from the server.
    //
    // When the names match what is already held — which is every refresh that
    // is not the first, because a library rescan is the only thing that adds a
    // genre — only the flags are updated and only the rows that actually
    // changed are repainted. A reset here would throw away the scroll position
    // in a 142-row dialog every time somebody ticked a box.
    void replace(const QList<RandomMix::Genre> &genres);

    // Optimistic local flips, applied so a checkbox responds within a frame.
    // The next replace() is authoritative and overwrites them.
    void setIncluded(const QString &name, bool included);
    void setAllIncluded(bool included);

Q_SIGNALS:
    void countChanged();
    void includedChanged();

private:
    bool sameNames(const QList<RandomMix::Genre> &genres) const;

    QList<RandomMix::Genre> m_genres;

    // Distinguishes "no genres" from "never asked". An empty list is a real
    // answer from a server with an empty library, and the two say opposite
    // things about whether the scope line has anything to report.
    bool m_loaded = false;
};

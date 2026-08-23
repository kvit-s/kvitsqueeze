// SPDX-License-Identifier: MPL-2.0

#pragma once

// prd.md FR-9.2: the in-app diagnostics panel — raw control traffic, the
// engine's own log lines, and a block of text to paste into a bug report.
//
// A bounded ring buffer, not a growing list. This receives every JSON-RPC
// body, every CLI event and every line squeezelite prints; an unbounded model
// would be a memory leak measured in hours (prd.md NFR-3).

#include <QAbstractListModel>
#include <QList>
#include <QString>

class ArtworkCache;
class LibraryController;
class LmsSession;

class DiagnosticsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool paused READ isPaused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Source { ControlOut, ControlIn, Engine, App };
    Q_ENUM(Source)

    enum Roles {
        TextRole = Qt::UserRole + 1,
        SourceRole,
        SourceNameRole,
        TimestampRole,
    };
    Q_ENUM(Roles)

    explicit DiagnosticsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isPaused() const { return m_paused; }
    void setPaused(bool paused);

    Q_INVOKABLE void clear();

    // Everything in the buffer as one block, with a header naming the app and
    // engine versions. This is the "copy diagnostics" button.
    Q_INVOKABLE QString asText() const;

public Q_SLOTS:
    void append(int source, const QString &text);
    void appendControl(bool outgoing, const QString &text);
    void appendEngine(const QString &text);

Q_SIGNALS:
    void pausedChanged();
    void countChanged();

private:
    struct Entry
    {
        QString text;
        Source source;
        qint64 msecs;
    };

    QList<Entry> m_entries;
    bool m_paused = false;
};

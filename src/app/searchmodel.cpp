#include "searchmodel.h"

#include "lmscommands.h"
#include "lmssession.h"

#include <QPointer>
#include <QTimer>

namespace {

// Long enough that a normal typing speed produces one request per word,
// short enough that it still feels incremental.
constexpr int kDebounceMs = 220;

// The server's own totals are reported separately, so this only bounds what is
// drawn. Sectioned results are for recognising the thing you meant, not for
// browsing — that is what the browse screens are.
constexpr int kResultsPerSection = 30;

// A one-character term matches most of a library and the reply is useless.
constexpr int kMinimumTermLength = 2;

} // namespace

SearchModel::SearchModel(LmsSession *session, QObject *parent)
    : QAbstractListModel(parent)
    , m_session(session)
    , m_debounce(new QTimer(this))
{
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, &SearchModel::run);
}

int SearchModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_rows.size());
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &row = m_rows.at(index.row());
    switch (static_cast<Roles>(role)) {
    case ItemIdRole:   return row.item.id;
    case TitleRole:    return row.item.title;
    case SubtitleRole: return row.item.subtitle;
    case SectionRole:  return int(row.section);
    case SectionNameRole:
        switch (row.section) {
        case ArtistSection: return tr("Artists");
        case AlbumSection:  return tr("Albums");
        case TrackSection:  return tr("Tracks");
        }
        return QString();
    case SelectorKeyRole:
        switch (row.section) {
        case ArtistSection: return QStringLiteral("artist_id");
        case AlbumSection:  return QStringLiteral("album_id");
        case TrackSection:  return QStringLiteral("track_id");
        }
        return QString();
    }
    return {};
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
    return {
        { ItemIdRole,      "itemId" },
        { TitleRole,       "title" },
        { SubtitleRole,    "subtitle" },
        { SectionRole,     "section" },
        { SectionNameRole, "sectionName" },
        { SelectorKeyRole, "selectorKey" },
    };
}

void SearchModel::setTerm(const QString &term)
{
    const QString trimmed = term.trimmed();
    if (trimmed == m_term)
        return;

    m_term = trimmed;
    Q_EMIT termChanged();

    if (m_term.size() < kMinimumTermLength) {
        m_debounce->stop();
        clear();
        return;
    }
    m_debounce->start();
}

void SearchModel::clear()
{
    m_debounce->stop();
    // Bumping the generation abandons anything in flight, so a reply for a
    // term the user has already cleared cannot repopulate the list.
    ++m_generation;
    if (m_pending > 0) {
        m_pending = 0;
        Q_EMIT searchingChanged();
    }

    if (m_rows.isEmpty() && m_reply.isEmpty())
        return;

    beginResetModel();
    m_rows.clear();
    m_reply = SearchReply();
    endResetModel();
    Q_EMIT resultsChanged();
}

void SearchModel::run()
{
    if (m_term.size() < kMinimumTermLength)
        return;

    const quint64 generation = ++m_generation;
    ++m_pending;
    if (m_pending == 1)
        Q_EMIT searchingChanged();

    QPointer<SearchModel> alive(this);
    m_session->send(LmsCommand::search(0, kResultsPerSection, m_term),
                    [this, alive, generation](const QJsonObject &result) {
                        if (!alive)
                            return;

                        if (--m_pending <= 0) {
                            m_pending = 0;
                            Q_EMIT searchingChanged();
                        }

                        // A slower earlier request answering after a newer one
                        // would otherwise show results for a term the user has
                        // already typed past.
                        if (generation != m_generation)
                            return;

                        m_reply = SearchReply::fromResult(result);
                        rebuild();
                    });
}

void SearchModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    for (const BrowseItem &item : m_reply.artists)
        m_rows.append({ item, ArtistSection });
    for (const BrowseItem &item : m_reply.albums)
        m_rows.append({ item, AlbumSection });
    for (const BrowseItem &item : m_reply.tracks)
        m_rows.append({ item, TrackSection });
    endResetModel();

    Q_EMIT resultsChanged();
}

#include "librarycontroller.h"

#include "lmscommands.h"
#include "lmssession.h"

#include <QJsonObject>
#include <QPointer>
#include <QTimer>

namespace {

constexpr int kRescanPollMs = 1500;

LmsCommand::QueueAction toQueueAction(int action)
{
    switch (static_cast<LibraryController::Action>(action)) {
    case LibraryController::PlayNow:  return LmsCommand::QueueAction::PlayNow;
    case LibraryController::PlayNext: return LmsCommand::QueueAction::PlayNext;
    case LibraryController::AddToEnd: return LmsCommand::QueueAction::AddToEnd;
    }
    return LmsCommand::QueueAction::AddToEnd;
}

} // namespace

LibraryController::LibraryController(LmsSession *session, QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_rescanPoll(new QTimer(this))
{
    m_rescanPoll->setInterval(kRescanPollMs);
    connect(m_rescanPoll, &QTimer::timeout, this, &LibraryController::pollRescan);

    connect(m_session, &LmsSession::stateChanged, this, [this](LmsSession::State state) {
        if (state == LmsSession::State::Connected)
            refreshServerInfo();
    });
}

BrowseModel *LibraryController::browse(int kind, const QStringList &filters)
{
    // No parent on purpose. A QObject returned from an invokable without a
    // parent is owned by the QML engine, so the model dies with the page that
    // holds it — which is the only lifetime a navigation stack can express.
    return new BrowseModel(m_session, static_cast<BrowseKind>(kind), filters);
}

BrowseModel *LibraryController::newMusic()
{
    return browse(Albums, { QStringLiteral("sort:new") });
}

BrowseModel *LibraryController::randomAlbums()
{
    return browse(Albums, { QStringLiteral("sort:random") });
}

void LibraryController::enqueue(const QString &selectorKey, const QString &selectorValue,
                                int action)
{
    if (selectorKey.isEmpty() || selectorValue.isEmpty())
        return;
    enqueueFiltered({ LmsCommand::param(selectorKey, selectorValue) }, action);
}

void LibraryController::enqueueFiltered(const QStringList &filters, int action)
{
    if (filters.isEmpty())
        return;

    m_session->send(LmsCommand::playlistControl(toQueueAction(action), filters));

    // The queue changed, and the CLI event for it will say so — but asking now
    // is what makes the queue drawer redraw inside the same interaction rather
    // than a notification cycle later.
    m_session->refreshQueue();
    m_session->refreshStatus();
}

void LibraryController::refreshServerInfo()
{
    QPointer<LibraryController> alive(this);
    m_session->sendServerScoped(LmsCommand::serverStatus(),
                                [this, alive](const QJsonObject &result) {
        if (!alive || result.isEmpty())
            return;

        m_serverVersion = result.value(QStringLiteral("version")).toString();
        m_albumCount = result.value(QStringLiteral("info total albums")).toInt();
        m_artistCount = result.value(QStringLiteral("info total artists")).toInt();
        m_trackCount = result.value(QStringLiteral("info total songs")).toInt();
        Q_EMIT serverInfoChanged();

        // A scan already running when the app starts should show its progress,
        // not appear only if this app was the one that asked for it.
        pollRescan();
    });
}

void LibraryController::rescan(bool playlistsOnly)
{
    m_session->sendServerScoped(LmsCommand::rescan(playlistsOnly));
    m_rescanning = true;
    m_rescanDetail = tr("Starting…");
    Q_EMIT rescanChanged();
    m_rescanPoll->start();
}

void LibraryController::pollRescan()
{
    QPointer<LibraryController> alive(this);
    m_session->sendServerScoped(LmsCommand::rescanProgress(),
                                [this, alive](const QJsonObject &result) {
        if (!alive)
            return;

        const bool running = result.value(QStringLiteral("rescan")).toInt() != 0;
        QString detail;
        if (running) {
            // The reply names whichever step is current and gives it as
            // "done/total"; which steps exist depends on the server version,
            // so the field names are read rather than assumed.
            const QString step = result.value(QStringLiteral("steps")).toString()
                                     .section(QLatin1Char(','), -1);
            const QString progress = result.value(step).toString();
            detail = progress.isEmpty() ? step : QStringLiteral("%1 %2").arg(step, progress);
        }

        if (running == m_rescanning && detail == m_rescanDetail)
            return;

        m_rescanning = running;
        m_rescanDetail = detail;
        Q_EMIT rescanChanged();

        if (running) {
            m_rescanPoll->start();
        } else {
            m_rescanPoll->stop();
            // The library just changed underneath every open browse screen.
            refreshServerInfo();
        }
    });
}

#include "appcontext.h"

#include "externalengine.h"
#include "lmssession.h"
#include "queuemodel.h"

AppContext::AppContext(const Options &options, QObject *parent)
    : QObject(parent)
    , m_options(options)
    , m_session(new LmsSession(this))
    , m_engine(new ExternalEngine(this))
    , m_queue(new QueueModel(this))
{
    connect(m_session, &LmsSession::statusReceived, this, [](const PlayerStatus &status) {
        // TODO (M2): reconcile into the models. prd.md FR-1.6 gives this a
        // 500 ms budget and makes the server the winner on conflict.
        Q_UNUSED(status)
    });
}

void AppContext::registerQmlTypes()
{
    // The `Sqz` module's types are declared in qmlsingletons.h; linking
    // sqz-qml is what makes them available. This hook exists for the
    // registrations that cannot be declarative — there are none yet, and the
    // function is kept so the call site in SqeezeAmpApplication does not have
    // to appear later.
}

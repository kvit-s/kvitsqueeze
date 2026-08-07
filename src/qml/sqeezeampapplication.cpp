#include "sqeezeampapplication.h"

#include "appcontext.h"

#include <QQmlContext>
#include <QQuickStyle>

SqeezeAmpApplication::SqeezeAmpApplication(QObject *parent)
    : QObject(parent)
    , m_context(new AppContext({}, this))
{
    // Basic rather than Fusion: the shell draws its own controls, and Basic
    // is the style that does not fight custom styling.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    AppContext::registerQmlTypes();
}

bool SqeezeAmpApplication::load()
{
    m_engine.rootContext()->setContextProperty(QStringLiteral("app"), m_context);
    m_engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    return !m_engine.rootObjects().isEmpty();
}

bool SqeezeAmpApplication::claimSingleInstance()
{
    // TODO (M4): QLocalServer lock plus an activation message that raises the
    // existing window. Named pipe only — see prd.md N7.
    return true;
}

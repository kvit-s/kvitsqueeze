#include "applog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

Q_LOGGING_CATEGORY(logProtocol, "sqz.protocol")
Q_LOGGING_CATEGORY(logSession, "sqz.session")
Q_LOGGING_CATEGORY(logEngine, "sqz.engine")
Q_LOGGING_CATEGORY(logUi, "sqz.ui")

namespace {

constexpr qint64 kMaxBytes = 2 * 1024 * 1024;
constexpr int kGenerations = 5;

QFile *g_file = nullptr;
QMutex g_mutex;
QtMessageHandler g_previous = nullptr;

QString logDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QStringLiteral("/logs");
}

QString logPath()
{
    return logDirectory() + QStringLiteral("/sqeezeamp.log");
}

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return "D";
    case QtInfoMsg:     return "I";
    case QtWarningMsg:  return "W";
    case QtCriticalMsg: return "E";
    case QtFatalMsg:    return "F";
    }
    return "?";
}

// Called with g_mutex held.
void rotateIfNeeded()
{
    if (!g_file || g_file->size() < kMaxBytes)
        return;

    g_file->close();

    // sqeezeamp.log.4 is dropped, 3 becomes 4, and so on. Renaming from the
    // oldest end is what keeps a generation from being overwritten before it
    // has been moved.
    const QString base = logPath();
    QFile::remove(base + QStringLiteral(".%1").arg(kGenerations - 1));
    for (int index = kGenerations - 2; index >= 1; --index) {
        QFile::rename(base + QStringLiteral(".%1").arg(index),
                      base + QStringLiteral(".%1").arg(index + 1));
    }
    QFile::rename(base, base + QStringLiteral(".1"));

    g_file->setFileName(base);
    g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void handler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    {
        QMutexLocker locker(&g_mutex);
        if (g_file && g_file->isOpen()) {
            rotateIfNeeded();

            QTextStream stream(g_file);
            stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                   << ' ' << levelName(type)
                   << ' ' << (context.category ? context.category : "default")
                   << ": " << message << '\n';
            stream.flush();
        }
    }

    // Keep the default behaviour as well: a debug build run from a console
    // should still print, and qFatal must still abort.
    if (g_previous)
        g_previous(type, context, message);
}

} // namespace

namespace AppLog {

void install()
{
    QDir().mkpath(logDirectory());

    g_file = new QFile(logPath());
    if (!g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // A player that will not start because it could not write a log file
        // is worse than a player with no log file.
        delete g_file;
        g_file = nullptr;
    }

    // Per-subsystem levels, as filter rules so they are Qt's own vocabulary
    // and QT_LOGGING_RULES in the environment can still override them for a
    // one-off debug session.
    //
    // Spelled out per category rather than as `sqz.*.debug=false`. A wildcard
    // rule that Qt declines to parse is dropped in silence, and the symptom is
    // a log file that exists and stays empty — which is indistinguishable from
    // the logging never being wired up at all.
    QSettings settings;
    const QString rules = settings.value(QStringLiteral("log/rules"),
                                         QStringLiteral("sqz.protocol.debug=false\n"
                                                        "sqz.session.debug=false\n"
                                                        "sqz.engine.debug=false\n"
                                                        "sqz.ui.debug=false"))
                              .toString();
    QLoggingCategory::setFilterRules(rules);

    g_previous = qInstallMessageHandler(handler);

    // The default category, on purpose: this line has to appear even when the
    // rules above have turned every subsystem off, because it is what says the
    // log file is working.
    qInfo().noquote() << "SqeezeAmp" << QCoreApplication::applicationVersion()
                      << "starting —" << logPath();
}

QString directory()
{
    return logDirectory();
}

QString currentFile()
{
    return logPath();
}

} // namespace AppLog

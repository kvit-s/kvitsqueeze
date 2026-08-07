#pragma once

// Owns the QML engine and the composed application, so that main() is a
// launcher and nothing more. Anything a second entry point would need to
// duplicate belongs here instead.

#include <QObject>
#include <QQmlApplicationEngine>

class AppContext;
class QGuiApplication;

class SqeezeAmpApplication : public QObject
{
    Q_OBJECT

public:
    explicit SqeezeAmpApplication(QObject *parent = nullptr);

    // Loads the shell. Returns false if QML failed to produce a window, which
    // main() turns into a non-zero exit rather than a process that runs with
    // no UI.
    bool load();

    AppContext *context() const { return m_context; }

    // Single instance (prd.md FR-7.3): returns false if another SqeezeAmp
    // already holds the lock, after asking it to raise its window. Uses a
    // named pipe — never a TCP port, not even on loopback (prd.md N7).
    static bool claimSingleInstance();

private:
    AppContext *m_context = nullptr;
    QQmlApplicationEngine m_engine;
};

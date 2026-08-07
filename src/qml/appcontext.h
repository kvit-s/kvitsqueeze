#pragma once

// The composition root: the one place the object graph is wired.
//
// Do not hand-build this graph anywhere else, tests included. A test that
// assembles its own session/engine/model set drifts from the real
// composition and then passes against a shape the app never has. If a test
// needs a different composition, it belongs in AppContext::Options.

// queuemodel.h rather than a forward declaration: moc needs the full type for
// a pointer Q_PROPERTY, and an incomplete one fails deep inside qmetatype.h
// with "Pointer Meta Types must either point to fully-defined types".
#include "queuemodel.h"

#include <QObject>

class ExternalEngine;
class LmsSession;

class AppContext : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QueueModel *queue READ queue CONSTANT)

public:
    struct Options
    {
        // Skip launching the audio engine. Used by the shell tests, which
        // exercise the UI against a session and have no business spawning a
        // real player onto the user's server.
        bool startEngine = true;
    };

    explicit AppContext(const Options &options = {}, QObject *parent = nullptr);

    LmsSession *session() const { return m_session; }
    ExternalEngine *engine() const { return m_engine; }
    QueueModel *queue() const { return m_queue; }

    // Called once before the QML engine loads anything. Registration is
    // explicit rather than a plugin because sqz-qml is linked directly into
    // every binary that uses it (NO_PLUGIN in CMakeLists.txt).
    static void registerQmlTypes();

private:
    Options m_options;
    LmsSession *m_session = nullptr;
    ExternalEngine *m_engine = nullptr;
    QueueModel *m_queue = nullptr;
};

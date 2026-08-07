#pragma once

// Every type QML can see, declared in one place.
//
// Types are registered as QML_FOREIGN wrappers here, never with a macro on
// the class itself. qmltyperegistrar reads ONE target's metatypes — sqz-qml's
// — so a QML_ELEMENT on a class in sqz-app or sqz-protocol is silently
// ignored at build time and shows up at runtime as
// "ReferenceError: <Type> is not defined". Putting the wrappers here keeps
// the lower modules free of QML dependencies and makes the QML surface a
// list somebody can read.

#include "queuemodel.h"

#include <QQmlEngine>

struct QueueModelForeign
{
    Q_GADGET
    QML_FOREIGN(QueueModel)
    QML_NAMED_ELEMENT(QueueModel)
    QML_UNCREATABLE("QueueModel is owned by AppContext and exposed as a property")
};

// SPDX-License-Identifier: MPL-2.0

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
//
// Every one of these is UNCREATABLE. QML never builds a piece of the player;
// AppContext does, once, and QML binds to what it made (prd.md §7.1). A
// creatable model here would be a second composition root.

#include "browsemodel.h"
#include "diagnosticsmodel.h"
#include "enginecontroller.h"
#include "librarycontroller.h"
#include "lyricscontroller.h"
#include "mixgenremodel.h"
#include "playbackcontroller.h"
#include "queuemodel.h"
#include "randommixcontroller.h"
#include "searchmodel.h"
#include "settings.h"

#include <QQmlEngine>

struct QueueModelForeign
{
    Q_GADGET
    QML_FOREIGN(QueueModel)
    QML_NAMED_ELEMENT(QueueModel)
    QML_UNCREATABLE("QueueModel is owned by AppContext and exposed as a property")
};

struct BrowseModelForeign
{
    Q_GADGET
    QML_FOREIGN(BrowseModel)
    QML_NAMED_ELEMENT(BrowseModel)
    QML_UNCREATABLE("Ask LibraryController for a BrowseModel; it owns the query")
};

struct SearchModelForeign
{
    Q_GADGET
    QML_FOREIGN(SearchModel)
    QML_NAMED_ELEMENT(SearchModel)
    QML_UNCREATABLE("SearchModel is owned by AppContext and exposed as a property")
};

// Registered for its enums as much as its properties: QML says
// Library.PlayNow and Library.Albums rather than carrying its own copy of
// the numbers.
struct LibraryControllerForeign
{
    Q_GADGET
    QML_FOREIGN(LibraryController)
    QML_NAMED_ELEMENT(Library)
    QML_UNCREATABLE("LibraryController is owned by AppContext")
};

struct PlaybackControllerForeign
{
    Q_GADGET
    QML_FOREIGN(PlaybackController)
    QML_NAMED_ELEMENT(Playback)
    QML_UNCREATABLE("PlaybackController is owned by AppContext")
};

struct EngineControllerForeign
{
    Q_GADGET
    QML_FOREIGN(EngineController)
    QML_NAMED_ELEMENT(Engine)
    QML_UNCREATABLE("EngineController is owned by AppContext")
};

struct SettingsForeign
{
    Q_GADGET
    QML_FOREIGN(Settings)
    QML_NAMED_ELEMENT(Settings)
    QML_UNCREATABLE("Settings is owned by AppContext")
};

// Registered for its enums as much as its properties: QML says Mix.Songs and
// Mix.Active rather than carrying its own copy of the numbers, and Mix.Unknown
// is the one that stops a three-state answer being written as a bool.
struct RandomMixControllerForeign
{
    Q_GADGET
    QML_FOREIGN(RandomMixController)
    QML_NAMED_ELEMENT(Mix)
    QML_UNCREATABLE("RandomMixController is owned by AppContext")
};

struct MixGenreModelForeign
{
    Q_GADGET
    QML_FOREIGN(MixGenreModel)
    QML_NAMED_ELEMENT(MixGenreModel)
    QML_UNCREATABLE("Ask RandomMixController for its genre scope")
};

// Registered for its enums as much as its properties: Lyrics.Absent and
// Lyrics.Unavailable are the two the pane must not draw the same way, and a
// QML file spelling them as numbers is how they would end up merged.
struct LyricsControllerForeign
{
    Q_GADGET
    QML_FOREIGN(LyricsController)
    QML_NAMED_ELEMENT(Lyrics)
    QML_UNCREATABLE("LyricsController is owned by AppContext")
};

struct DiagnosticsModelForeign
{
    Q_GADGET
    QML_FOREIGN(DiagnosticsModel)
    QML_NAMED_ELEMENT(Diagnostics)
    QML_UNCREATABLE("DiagnosticsModel is owned by AppContext")
};

cmake_minimum_required(VERSION 3.21)
project(sqeezeamp VERSION 0.1.0 LANGUAGES CXX)

# The full SemVer string the binary reports, prerelease suffix included.
# PROJECT_VERSION carries the three numeric components only, because CMake
# rejects a prerelease suffix there. A tag build passes the version derived
# from the tag, so v0.2.0-rc1 produces a binary that calls itself 0.2.0-rc1.
set(SQZ_VERSION_FULL "${PROJECT_VERSION}" CACHE STRING
    "Full SemVer version the binary reports (e.g. 0.2.0-rc1)")

if(NOT SQZ_VERSION_FULL MATCHES "^${PROJECT_VERSION}([-+].+)?$")
    message(FATAL_ERROR
        "SQZ_VERSION_FULL ('${SQZ_VERSION_FULL}') must be PROJECT_VERSION "
        "('${PROJECT_VERSION}') optionally followed by a '-prerelease' or "
        "'+build' suffix.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Widgets is not optional despite this being a QML app: QSystemTrayIcon lives
# there, and prd.md FR-7.1 makes the tray a P0 requirement. Network is listed
# because sqz-session links it; see the ownership rule below.
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Widgets Network Concurrent Test QuickTest)

# Generated qmldir/.qmltypes land in `:/qt/qml/Sqz`, Qt's standard location
# and on the engine's default import path. Unset, every configure warns.
qt_policy(SET QTP0001 NEW)

# ── The module boundary
#
# Five static libraries, one directory under src/ each, linked in one
# direction only:
#
#   sqz-protocol    pure LMS protocol: JSON-RPC request bodies, reply and CLI
#                   line parsing, player-status diffing. No sockets, no files,
#                   no Qt GUI. Every function is input → output, which is what
#                   makes M1 testable before any of the app exists.
#         ↑
#   sqz-session     the live connection: discovery, HTTP transport, the CLI
#                   event socket, reconnect and backoff. The ONLY module that
#                   links Qt6::Network.
#         ↑
#   sqz-app         orchestration and the QML-facing models: queue, browse,
#                   search, artwork cache, and the playback coordinator that
#                   owns the engine.
#         ↑            ↑
#   sqz-engine  ─────┘  audio engine behind IAudioEngine. For v1 that is
#                   ExternalEngine, which supervises a squeezelite.exe child
#                   process (prd.md §7.3.2). The ONLY module that links
#                   QProcess. Depends on nothing but Qt Core — it is handed a
#                   config struct and told to start, and knows nothing about
#                   LMS, models or the UI.
#         ↑
#   sqz-qml         the presentation surface: the `Sqz` QML module's type
#                   registrations, and the composition root (AppContext,
#                   SqeezeAmpApplication) that wires the object graph and
#                   hands it to a QQmlEngine. A composition root depends on
#                   everything by definition, which is why it sits on top.
#
# The direction is enforced, not documented: each target publishes only its
# own directory as an include path and inherits the directories of what it
# links, so a protocol source writing #include "lmssession.h" does not find
# the header and does not compile. tools/check-layering.py checks the same
# graph over the sources, plus the two ownership rules the include graph
# cannot express — Qt Network only in sqz-session, QProcess only in
# sqz-engine — and one product invariant, that no call site outside
# sqz-session may supply a player id (prd.md FR-6.1).

# The shipped app is one statically-linked executable. Shared is offered for
# development because every test binary links sqz-core and therefore the whole
# chain; shared gives each test its own object and one copy of the rest.
option(SQZ_SHARED_LIBS
       "Build the sqz-* modules as shared libraries (development builds)" OFF)
if(SQZ_SHARED_LIBS)
    set(SQZ_LIB_TYPE SHARED)
else()
    set(SQZ_LIB_TYPE STATIC)
endif()

# ── sqz-protocol — the wire format, and nothing else.
add_library(sqz-protocol ${SQZ_LIB_TYPE}
    src/protocol/lmsrequest.cpp
    src/protocol/playerstatus.cpp
)
target_link_libraries(sqz-protocol PUBLIC Qt6::Core)
target_include_directories(sqz-protocol PUBLIC src/protocol)

# ── sqz-session — the live connection.
#
# Qt6::Network is linked here and nowhere else. That is the ownership rule in
# build form: a module above this one cannot open a connection because it has
# nothing to open one with. PUBLIC because the session's own headers hold a
# QNetworkAccessManager.
add_library(sqz-session ${SQZ_LIB_TYPE}
    src/session/lmssession.cpp
)
target_link_libraries(sqz-session PUBLIC sqz-protocol Qt6::Network)
target_include_directories(sqz-session PUBLIC src/session)

# ── sqz-engine — the audio engine behind IAudioEngine.
#
# Deliberately depends on no other sqz module. It is configured with a struct
# (server address, player id, device name) and reports status back; it does
# not know what LMS is. That is what keeps prd.md §7.3's backend swap a
# one-class change rather than a refactor.
add_library(sqz-engine ${SQZ_LIB_TYPE}
    src/engine/externalengine.cpp
    src/engine/iaudioengine.h
)
target_link_libraries(sqz-engine PUBLIC Qt6::Core)
target_include_directories(sqz-engine PUBLIC src/engine)

# ── sqz-app — orchestration and the QML-facing models.
add_library(sqz-app ${SQZ_LIB_TYPE}
    src/app/queuemodel.cpp
)
target_link_libraries(sqz-app PUBLIC sqz-session sqz-engine)
target_link_libraries(sqz-app PRIVATE Qt6::Concurrent Qt6::Gui)
target_include_directories(sqz-app PUBLIC src/app)

# ── sqz-qml — the presentation surface and the composition root.
add_library(sqz-qml ${SQZ_LIB_TYPE}
    src/qml/appcontext.cpp
    src/qml/sqeezeampapplication.cpp
    src/qml/qmlsingletons.h
)
# Quick is PUBLIC because sqeezeampapplication.h holds a QQmlApplicationEngine
# by value, which puts Qt Qml in this module's public surface rather than its
# implementation. QuickControls2 for the shell's style and Widgets because
# SqeezeAmpApplication is a QApplication are both genuinely private.
target_link_libraries(sqz-qml PUBLIC sqz-app Qt6::Quick)
target_link_libraries(sqz-qml PRIVATE Qt6::QuickControls2 Qt6::Widgets)
target_include_directories(sqz-qml PUBLIC src/qml)

target_compile_definitions(sqz-qml PRIVATE SQZ_VERSION="${SQZ_VERSION_FULL}")

# The `Sqz` QML module. Types are declared as QML_FOREIGN wrappers in
# src/qml/qmlsingletons.h — never with a macro on the class itself — so a
# class in any module can reach QML while only this one links the registration
# machinery. qmltyperegistrar reads ONE target's metatypes, so a macro
# elsewhere is silently ignored and QML fails at runtime with
# "ReferenceError: <Type> is not defined".
#
# NO_PLUGIN: sqz-qml is linked directly into every binary that uses it, so
# there is nothing for a plugin to load.
qt_add_qml_module(sqz-qml
    URI Sqz
    VERSION 1.0
    NO_PLUGIN
    DEPENDENCIES QtQuick
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qmlmodules/Sqz
)

# Build this when all you want is the `Sqz` module description qmllint reads.
# Qt's own generator target is named after whichever target
# qt_add_qml_module() was attached to, so it moves whenever the module does.
# Nothing outside this file should name Qt's target.
add_custom_target(sqz-qmltypes ALL)
add_dependencies(sqz-qmltypes sqz-qml_qmltyperegistration)

# The `Sqz` module registers types compiled into the modules below this one.
# qmltyperegistrar resolves a QML_FOREIGN wrapper against the metatype
# description of the class it names, and reads those from the metatypes.json
# files that reach sqz-qml through its link interface. Extracting them is not
# automatic for a plain add_library, so each module asks for it here.
foreach(_module protocol session engine app)
    qt_extract_metatypes(sqz-${_module})
endforeach()

# Those descriptions must exist before the registrar reads them, which under
# the Visual Studio generator does not happen on its own: Qt creates a
# standalone _qmltyperegistration target carrying a second copy of the
# registration rule with no dependency on the fill, so the registrar runs
# against zero-byte files and dies with "Failed to parse JSON" — a message no
# log carries, because qmltyperegistrar is a GUI-subsystem binary on Windows
# and puts its diagnostics in a message box a CI runner cannot show.
if(CMAKE_GENERATOR MATCHES "Visual Studio" AND TARGET sqz-qml_qmltyperegistration)
    add_dependencies(sqz-qml_qmltyperegistration sqz-qml)
endif()

# ── Warnings, on the modules this repository writes.
#
# C4062 (MSVC) / -Werror=switch: an enumerator is not handled and there is no
# default label. A switch over a player state or an engine status that has
# forgotten an enumerator is not a style question — it is a state being
# silently dropped from whatever that switch decides.
if(MSVC)
    set(SQZ_WARNING_FLAGS /W3 /we4062)
else()
    set(SQZ_WARNING_FLAGS -Wall -Wextra -Werror=switch)
endif()
foreach(_target IN ITEMS sqz-protocol sqz-session sqz-engine sqz-app sqz-qml)
    target_compile_options(${_target} PRIVATE ${SQZ_WARNING_FLAGS})
endforeach()

# ── sqz-core — "the player", as one name.
#
# An interface target contributing no objects of its own: the five modules
# under one label, so main() and every test link one thing.
add_library(sqz-core INTERFACE)
target_link_libraries(sqz-core INTERFACE sqz-qml)

qt_add_executable(sqeezeamp
    src/main.cpp
    resources.qrc
)
target_link_libraries(sqeezeamp PRIVATE
    sqz-core
    # main.cpp constructs the QApplication itself, so the executable needs
    # Widgets directly rather than inheriting it from sqz-core.
    Qt6::Widgets
)
# main.cpp reports the version too, and the define on sqz-qml is PRIVATE.
target_compile_definitions(sqeezeamp PRIVATE SQZ_VERSION="${SQZ_VERSION_FULL}")

if(WIN32)
    set_target_properties(sqeezeamp PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

include(GNUInstallDirs)

# ── The licence payload every binary artifact must carry.
#
# This is a distribution obligation, not documentation (prd.md §11.2). v1
# ships stock squeezelite.exe as a child process; bundling that GPLv3 binary
# requires its licence text and a written offer for its source to travel with
# it, whatever licence this application itself carries. Qt's LGPL texts come
# from the Qt kit at deploy time and are added by win-deploy.bat.
set(SQZ_LICENSE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/licenses/LICENSE.squeezelite"
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/licenses/WRITTEN-OFFER.txt")

# A missing file here is a configure-time error rather than a package that
# quietly ships without its notices.
foreach(_lic IN LISTS SQZ_LICENSE_FILES)
    if(NOT EXISTS "${_lic}")
        message(FATAL_ERROR
            "Required licence file is missing: ${_lic}. Every binary artifact "
            "must ship it; see SQZ_LICENSE_FILES in CMakeLists.txt and "
            "prd.md §11.2.")
    endif()
endforeach()

if(WIN32)
    # Portable layout: the executable at the prefix root, windeployqt fills in
    # the Qt runtime beside it, and squeezelite.exe is staged into engine/.
    install(TARGETS sqeezeamp RUNTIME DESTINATION .)
    install(FILES ${SQZ_LICENSE_FILES} DESTINATION licenses)
else()
    install(TARGETS sqeezeamp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${SQZ_LICENSE_FILES}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/sqeezeamp)
endif()

# Only when this project is the top-level build.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()

find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)

# Labels decide what runs where (CMakePresets.json):
#   unit   deterministic, no display, no network, no child processes
#   shell  needs a QML engine and a window
#
# A test that talks to a real server or launches a real squeezelite.exe
# belongs to neither: it is a manual check, not a suite entry.

# Every test links sqz-core, which is the whole chain under one name, so a
# test cannot accidentally exercise a different composition from the app's.
function(sqz_add_test name source label)
    qt_add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE sqz-core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS ${label})
endfunction()

# ── Protocol: request building, the command vocabulary, reply parsing.
#
# The whole point of sqz-protocol being pure is that this suite can pin the
# wire format down before any of the app exists (prd.md M1). Every case in
# these was checked against Lyrion Music Server 9.1.0 before it was written.
sqz_add_test(LmsRequestTests     test_lmsrequest.cpp     unit)
sqz_add_test(LmsCommandTests     test_lmscommands.cpp    unit)
sqz_add_test(PlayerStatusTests   test_playerstatus.cpp   unit)
sqz_add_test(BrowseReplyTests    test_browsereply.cpp    unit)
sqz_add_test(CliEventTests       test_clievent.cpp       unit)
sqz_add_test(PlayerIdentityTests test_playeridentity.cpp unit)

# ── The single-player invariant, checked against the traffic.
#
# prd.md FR-6.1 says no request may escape with a foreign player id, and the
# layering guard below can only check spelling. This one reads the bodies.
sqz_add_test(LmsSessionTests     test_lmssession.cpp     unit)

# ── "The server wins" (prd.md FR-1.6 / FR-6.4).
#
# Optimistic update, the 500 ms reconciliation window, and the locally
# interpolated position. Drives PlaybackController against an unconfigured
# session, so it opens nothing.
sqz_add_test(PlaybackControllerTests test_playbackcontroller.cpp unit)

# ── Engine argument construction and log scraping.
#
# buildArguments() is the entire interface to the child process, and
# applyLogLine() is the whole of FR-2.5 under Backend B. Launches nothing.
sqz_add_test(ExternalEngineTests test_externalengine.cpp unit)

# ── The shell.
#
# Instantiates the real QML tree through the real composition root, with the
# engine and the session switched off. QML's failures are late and quiet, so
# this is what turns a missing resources.qrc entry into a red test.
sqz_add_test(ShellTests test_shell.cpp shell)

# The .qml files live in resources.qrc, which is compiled into whichever
# binary lists it — and that is normally only the application. Without this
# the test binary has no qrc:/qml/Main.qml at all and would report the shell
# as missing rather than as broken.
target_sources(ShellTests PRIVATE ${CMAKE_SOURCE_DIR}/resources.qrc)

# QuickControls2 for QQuickStyle: sqz-qml links it privately, because setting
# the style is the composition root's job and no other module has any business
# doing it. The test sets the same style the app does so the shell it
# instantiates is the shell that ships.
target_link_libraries(ShellTests PRIVATE Qt6::QuickControls2)

# Same reason as the application: without this the `Sqz` types are registered
# by an object file the linker drops, and the shell loads with an empty module.
sqz_keep_qml_registration(ShellTests)

# ── The module boundary and the ownership rules CMake cannot express.
#
# Runs tools/check-layering.py over the sources. Skipped rather than failed
# when Python is absent, because a missing interpreter is a machine problem
# and not a defect in the tree.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME LayeringGuard
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_SOURCE_DIR}/tools/check-layering.py)
    set_tests_properties(LayeringGuard PROPERTIES LABELS unit)
else()
    message(WARNING "Python 3 not found; the LayeringGuard test is not registered")
endif()

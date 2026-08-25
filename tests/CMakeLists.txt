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
sqz_add_test(SongInfoTests       test_songinfo.cpp       unit)

# ── The Random Mix's wire format (prd.md FR-3.9).
#
# Two of these cases are the difference between an honest indicator and a
# confident wrong one: a JSON null answer means "no mix", a missing answer
# means "nobody knows", and the fixtures here are real replies from Lyrion
# Music Server 9.1.0 so the distinction cannot be reasoned away.
sqz_add_test(RandomMixTests      test_randommix.cpp      unit)

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

# ── The mix a plugin can end without telling anyone (prd.md FR-3.9).
#
# The plugin emits no event when a mix starts or stops, and it stops its own
# mix when it sees a load go past — including this app's own "play now". So
# the state is polled, "unknown" is a state, and a mix that ended by itself
# has to be noticed rather than announced. Drives the controller against an
# unconfigured session, so it opens nothing.
sqz_add_test(RandomMixControllerTests test_randommixcontroller.cpp unit)

# ── The lyric sheet, and the absence that is not a fact (prd.md FR-5.5).
#
# LMS omits the field for a file that carries no lyrics, so "this file has
# none" and "the request failed" arrive as the same silence. Everything else
# here follows from keeping them apart. Drives the controller against an
# unconfigured session, so it opens nothing.
sqz_add_test(LyricsControllerTests test_lyricscontroller.cpp unit)

# ── The timed half of the same requirement.
#
# The timings are not on the wire at all: LMS serves the file's plain USLT tag
# and reads no `.lrc` beside it. Locating that sidecar from the server's own
# path, and reading it, are pure functions so the whole path is testable
# without a share to mount.
sqz_add_test(LrcSheetTests test_lrcsheet.cpp unit)

# ── Engine argument construction and log scraping.
#
# buildArguments() is the entire interface to the child process, and
# applyLogLine() is the whole of FR-2.5 under Backend B. Launches nothing.
sqz_add_test(ExternalEngineTests test_externalengine.cpp unit)

# ── Getting an engine in the first place (prd.md FR-2.11).
#
# The manifest format is a contract shared with the Inno Setup installer and
# fetch-engine.ps1, and the search order decides whether the engine the
# installer staged is the one that runs. Both are pure; nothing here opens a
# connection or writes to the real install folder.
sqz_add_test(EngineInstallerTests test_engineinstaller.cpp unit)

# ── The shell.
#
# Instantiates the real QML tree through the real composition root, with the
# engine and the session switched off. QML's failures are late and quiet, so
# this is what turns a missing resources.qrc entry into a red test.
sqz_add_test(ShellTests test_shell.cpp shell)

# ── The taskbar thumbnail toolbar's one portable decision (prd.md FR-7.7).
#
# The rest of that class is Win32 and needs a taskbar button to hover. This
# covers which glyph the middle button offers, which is the part that can be
# wrong without looking wrong.
sqz_add_test(TaskbarButtonsTests test_taskbarbuttons.cpp unit)

# ── When an automatic pause is allowed to undo itself (prd.md FR-7.11).
#
# The watcher half needs a microphone and a person to talk into it. The policy
# half is where the feature can be wrong without looking wrong — resuming over
# a play the user pressed themselves, or letting a bar of music in between two
# halves of one dictation — so it is a pure state machine and this drives it.
sqz_add_test(MicPauseTests test_micpause.cpp unit)

# ── What the single-instance pipe will accept (prd.md FR-7.10).
#
# The pipe is the process's only listening endpoint and anything running as
# this user can write to it, so the vocabulary is the security boundary.
# parseCommand() is static and pure for exactly this reason; the pipe itself
# needs a running app and stays a manual check.
sqz_add_test(SingleInstanceTests test_singleinstance.cpp unit)

# ── The one interaction that is worth real mouse events.
#
# ShellTests proves every view loads; it cannot prove a control does anything.
# prd.md FR-5.2's seek shipped broken twice, both times from a wrong reading of
# how a Slider delivers a release, and both times invisible to a suite that
# only instantiates. This one drags the real SeekBar and reads what came out.
sqz_add_test(SeekBarTests test_seekbar.cpp shell)
target_sources(SeekBarTests PRIVATE ${CMAKE_SOURCE_DIR}/resources.qrc)
target_link_libraries(SeekBarTests PRIVATE Qt6::QuickControls2 Qt6::Quick)

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

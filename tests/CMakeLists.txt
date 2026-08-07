find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)

# Labels decide what runs where (CMakePresets.json):
#   unit   deterministic, no display, no network, no child processes
#   shell  needs a QML engine and a window
#
# A test that talks to a real server or launches a real squeezelite.exe
# belongs to neither: it is a manual check, not a suite entry.

# ── Protocol: request building and reply parsing.
#
# The whole point of sqz-protocol being pure is that this suite can pin the
# wire format down before any of the app exists (prd.md M1).
qt_add_executable(test_lmsrequest test_lmsrequest.cpp)
target_link_libraries(test_lmsrequest PRIVATE sqz-core Qt6::Test)
add_test(NAME LmsRequestTests COMMAND test_lmsrequest)
set_tests_properties(LmsRequestTests PROPERTIES LABELS unit)

qt_add_executable(test_playerstatus test_playerstatus.cpp)
target_link_libraries(test_playerstatus PRIVATE sqz-core Qt6::Test)
add_test(NAME PlayerStatusTests COMMAND test_playerstatus)
set_tests_properties(PlayerStatusTests PROPERTIES LABELS unit)

# ── Engine argument construction.
#
# buildArguments() is the entire interface to the child process, so it is the
# thing worth pinning down. Launches nothing.
qt_add_executable(test_externalengine test_externalengine.cpp)
target_link_libraries(test_externalengine PRIVATE sqz-core Qt6::Test)
add_test(NAME ExternalEngineTests COMMAND test_externalengine)
set_tests_properties(ExternalEngineTests PROPERTIES LABELS unit)

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

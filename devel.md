# Building SqeezeAmp

Everything needed to get from a clone to a running build, and from a running
build to the two shippable artifacts.

This is the mechanical half. [`CONTRIBUTING.md`](CONTRIBUTING.md) is the other
half — the settled scope decisions, the module boundary, the invariants and the
traps that cost an afternoon each. Read that before changing behaviour; read
this before running anything.

## What you need

| | |
|---|---|
| **Windows 10/11 x64** | The only supported target. The code stays portable, but nothing else is built or tested. |
| **Qt 6.10.1, `msvc2022_64`** | Modules: Core, Gui, Quick, QuickControls2, Widgets, Network, Concurrent, Test, QuickTest. |
| **Visual Studio 2022** | Community is fine. The build uses the CMake that ships inside it. |
| **Python 3** | Optional. Only used by the `LayeringGuard` test; without it that one test is skipped with a warning. |
| **A running LMS** | Anywhere on your network. Developed against Lyrion 9.1.0 running as a Home Assistant add-on. |
| **`squeezelite.exe`** | Not included, and not committed — see [The audio engine](#the-audio-engine-for-a-development-build) below. Without it the app builds, runs and browses, but will not play. |

Both toolchain paths can be overridden in the environment if yours differ:

```bat
set QT_ROOT_DIR=C:\Qt\6.10.1\msvc2022_64
set VS_CMAKE_DIR=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
```

## Build

From a normal `cmd` prompt at the repository root — no Developer Command
Prompt needed, the scripts find the toolchain themselves:

```bat
win-build.bat                 :: configure + build (Release)
win-build.bat configure       :: configure only
win-build.bat build           :: build only, skipping the configure step
```

Output lands in `build-windows-msvc-release\Release\sqeezeamp.exe`.

> **Close the app before rebuilding.** A locked `sqeezeamp.exe` fails its link
> with `LNK1104` while everything else succeeds, which leaves a stale
> executable that looks freshly built.

## Test

```bat
win-test.bat                  :: everything, output to win-test-result.txt
```

**Read `win-test-result.txt`, not the console.** Piping ctest through another
process leaves stdout fully buffered, so output vanishes on a crash and
phantom failures appear.

Two labels, both run by the script:

- **`unit`** — deterministic. No display, no network, no child processes.
  Covers the protocol vocabulary, reply parsing, the engine's command line and
  log scraping, and the two rules that matter most: that no request can escape
  with another player's id, and that the server wins a conflict within 500 ms.
- **`shell`** — needs a QML engine and a window. Loads the real `Main.qml`
  through the real composition root with the session and the audio engine
  switched off, and instantiates every view.

To run one label, or one test, call `ctest` directly — it lives in
`%VS_CMAKE_DIR%` beside the CMake the build uses, and needs `%QT_ROOT_DIR%\bin`
on `PATH` so the test binaries find their Qt DLLs:

```bat
set PATH=%QT_ROOT_DIR%\bin;%PATH%
ctest --test-dir build-windows-msvc-release -C Release -L unit --output-on-failure
ctest --test-dir build-windows-msvc-release -C Release -R LmsSessionTests -V
```

## Run what you built

Run `win-deploy.bat` **once** after the first build — it copies the Qt runtime
next to the executable, without which nothing starts — then:

```bat
win-deploy.bat
build-windows-msvc-release\Release\sqeezeamp.exe
```

You only need to repeat the deploy step when the Qt version or the audio engine
changes; ordinary rebuilds just overwrite `sqeezeamp.exe` in place.

## The audio engine, for a development build

SqeezeAmp supervises a stock `squeezelite.exe` as a child process. It is
neither committed to this repository nor shipped in a release — see
[Audio engine](README.md#audio-engine) in the README for what users get
and why. For a development build, fetch one with the same script the portable
zip carries:

```bat
powershell -ExecutionPolicy Bypass -File packaging\windows\fetch-engine.ps1 -DestDir packaging\windows
win-deploy.bat
```

Or put your own copy at `packaging\windows\squeezelite.exe` (gitignored), or
point `SQZ_ENGINE_EXE` at it. Either way `win-deploy.bat` stages it into
`engine\` beside the application so the tree runs from Explorer, and
`win-package.bat` then removes it, because neither shipped artifact may carry
it.

**Treat an engine upgrade as a change with a test pass behind it.**
`ExternalEngine::applyLogLine()` scrapes a log format upstream makes no
promises about, and `ExternalEngineTests` holds lines captured from the pinned
build so a change shows up as a red test rather than as an empty diagnostics
panel. The build that was tested, with its checksum, is pinned in
[`packaging/engine-version.txt`](packaging/engine-version.txt); the location it
is fetched from is [`packaging/engine-manifest.txt`](packaging/engine-manifest.txt),
and that file may only ever be pointed at a build somebody has actually run.

## Package

```bat
win-deploy.bat                :: stage Qt, the licences and a local engine
win-package.bat               :: portable zip + installer  → dist\
win-package.bat zip           :: portable zip only
```

`win-deploy.bat` runs `windeployqt` and copies the licence files into the build
output, which is also what makes the executable runnable from Explorer.
`win-package.bat` then builds:

- `dist\SqeezeAmp-<version>-windows-x64.zip`
- `dist\SqeezeAmp-<version>-setup.exe`, if
  [Inno Setup 6](https://jrsoftware.org/isinfo.php) is installed — otherwise it
  builds the zip and says so.
- `dist\SHA256SUMS-windows.txt`

Both artifacts come from the same staged tree, so what ships is what you ran —
minus two things that are removed on the way out: the audio engine, which is
GPLv3 and is not distributed, and the build's own leavings (import libraries,
`.exp`, `make-appicon.exe`).

## Layout

```
src/protocol/   pure LMS protocol — requests, replies, the command vocabulary
src/session/    the live connection; the only module that links Qt Network
src/engine/     the audio engine behind IAudioEngine; the only module with QProcess
src/app/        orchestration and the QML-facing models
src/qml/        QML type registrations, the composition root, Windows integration
qml/            the shell's .qml files, shipped via resources.qrc
tests/          Qt Test; the `unit` and `shell` labels
tools/          check-layering.py — the module graph, enforced
```

Includes only ever point downward, and three ownership rules the compiler
cannot express are enforced by `tools/check-layering.py`, which runs as a test.
[`CONTRIBUTING.md`](CONTRIBUTING.md) explains each one, and where a new file
goes.

## Licensing, in the detail a contributor needs

The short version is in the [README](README.md#licence). The parts that affect
what you may write here:

**SqeezeAmp is [MPL-2.0](LICENSE).** Source files carry an
`SPDX-License-Identifier: MPL-2.0` line, and MPL Exhibit A's `LICENSE`-file
fallback covers the few that do not — the build scripts, the packaging inputs
and the documents.

MPL is file-level copyleft: a Larger Work built around these files can carry
whatever terms you like, but modifications to *these* files stay under MPL.

**Exhibit B, the "Incompatible With Secondary Licenses" notice, is not used
anywhere and must not be added.** Without it, MPL-2.0 §3.3 lets these files also
be distributed under a Secondary License — GPLv2-or-later, LGPLv2.1-or-later or
AGPLv3 — when combined with a work governed by one. That is what keeps an
otherwise awkward question academic rather than risky: whether supervising a
GPLv3 `squeezelite.exe` as a child process makes one work or two has no settled
answer, and if it were ever held to be one work, the combination can simply be
distributed under GPLv3.

**The engine is GPLv3 and is not distributed.** The installer fetches it from
upstream, so there is no corresponding source to attach to a release and no
written offer to make. Its licence text ships anyway — `packaging/licenses/`,
and `CMakeLists.txt` refuses to configure without it — so a user whose installer
fetched the engine has its terms on disk. What keeps this arrangement intact is
the arm's-length rule: talk to the child only through documented arguments and
its log output, and never patch it.

**Qt is used under LGPLv3**, which is why the Qt libraries ship as separate DLLs
and are never linked statically: a user must be able to relink against their own
Qt build. No static Qt in a shipped installer.

A build that never leaves the machine that produced it owes none of this.
GPLv3's obligations attach to distribution, not to use.
[`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md) is the file that has to stay
true when any of it changes.

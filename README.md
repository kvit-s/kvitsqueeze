# SqeezeAmp

A native Windows player for [Lyrion Music Server](https://lyrion.org) (LMS,
formerly Logitech Media Server). One Qt 6 / C++20 application that registers
itself as a SqueezeBox player, browses your library, and plays to a local audio
device — with a real desktop UI rather than the server's web skin in a browser
control.

**It controls exactly one player: itself.** Other players on your server —
hardware Squeezeboxes, other squeezelite instances, bridges — are not listed,
not selectable, and not controllable. This is a music player for this PC, not a
remote control for the house.

See [`prd.md`](prd.md) for what it is and is not, and
[`prd-progress.md`](prd-progress.md) for what is actually verified today.
Short version: it browses, queues and plays against a real server; most of the
Windows integration is built but has not been exercised by hand.

---

## What you need

| | |
|---|---|
| **Windows 10/11 x64** | The only supported target. The code stays portable, but nothing else is built or tested. |
| **Qt 6.10.1, `msvc2022_64`** | Modules: Core, Gui, Quick, QuickControls2, Widgets, Network, Concurrent, Test, QuickTest. |
| **Visual Studio 2022** | Community is fine. The build uses the CMake that ships inside it. |
| **Python 3** | Optional. Only used by the `LayeringGuard` test; without it that one test is skipped with a warning. |
| **A running LMS** | Anywhere on your network. Developed against Lyrion 9.1.0 running as a Home Assistant add-on. |
| **`squeezelite.exe`** | Not included — see [The audio engine](#the-audio-engine) below. Without it the app builds, runs and browses, but will not play. |

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

## Run

Run `win-deploy.bat` **once** after the first build — it copies the Qt runtime
next to the executable, without which nothing starts — then:

```bat
win-deploy.bat
build-windows-msvc-release\Release\sqeezeamp.exe
```

You only need to repeat the deploy step when the Qt version or the audio engine
changes; ordinary rebuilds just overwrite `sqeezeamp.exe` in place.

### First run

The app starts with **no server configured** and says so in a banner under the
title bar. Open **Settings** in the left rail and type your server's host and
port (default `9000`).

**Discovery will probably find nothing, and that is normal.** It broadcasts on
UDP 3483, and a broadcast does not cross a router — so a server on another
subnet, which includes any LMS running as a Home Assistant add-on, has to be
typed in. Auth is optional; if your server needs it, the password goes to the
Windows Credential Manager and never into the settings.

Once connected the app registers itself as a player. It appears in your
server's player list under whatever name is set in **Settings → Player**,
defaulting to `SqeezeAmp (<your-machine>)`, and it keeps the same identity
across restarts — so its queue and per-player settings survive on the server.

### Command line

```
sqeezeamp.exe --minimized     start hidden in the system tray

sqeezeamp.exe --play-pause    hand a transport command to the running copy
sqeezeamp.exe --next          and exit; does nothing if none is running
sqeezeamp.exe --previous
sqeezeamp.exe --stop
```

`--minimized` is what the "Start with Windows" setting writes into the Run key.
`--help` and `--version` are accepted but answer in a **message box** rather
than on stdout: this is a GUI-subsystem binary with no console attached, so Qt
has nowhere else to put them.

A second launch does not start a second copy: it raises the window of the one
already running and exits. The transport flags use that same mechanism — see
*Driving it from a script* below.

### Driving it from a script

For a keyboard whose keys can be remapped but which has no media keys — a
Microsoft Sculpt, say — a remapped key can run `sqeezeamp.exe --next`.

Launching a process per keypress costs a few hundred milliseconds of Qt
start-up, so `tools/sqz-remote.au3` talks to the running app directly instead:

```
sqz-remote.au3 next          also: previous, playpause, stop, activate
```

Both routes end at the same place — the single-instance named pipe,
`\\.\pipe\SqeezeAmp-instance-<username>`, which accepts those five verbs and
ignores everything else. It is the app's only listening endpoint and it is
**not** a socket, which is what keeps the no-remote-control rule (prd.md N7)
intact. Two consequences worth knowing:

- The pipe is per-user, so anything running as you can send a verb. That is the
  same trust boundary as you pressing a media key, and it is the whole of the
  surface: verbs only, nothing readable, no player id, no queries.
- It reaches **this** app specifically. Unlike a media key, which whichever
  player grabbed the hotkey first will answer, this cannot go to the wrong one.

### Keyboard

| | |
|---|---|
| <kbd>Space</kbd> | Play / pause |
| <kbd>←</kbd> <kbd>→</kbd> | Seek ∓5 s |
| <kbd>Ctrl</kbd>+<kbd>←</kbd> <kbd>→</kbd> | Previous / next track |
| <kbd>↑</kbd> <kbd>↓</kbd> | Volume |
| <kbd>Ctrl</kbd>+<kbd>F</kbd> | Search |
| <kbd>Ctrl</kbd>+<kbd>U</kbd> | Queue |
| <kbd>Ctrl</kbd>+<kbd>M</kbd> | Mini player |
| <kbd>Ctrl</kbd>+<kbd>,</kbd> | Settings |
| <kbd>Esc</kbd> | Close the lyrics, or go back |

The four media keys work globally, whether or not the window has focus.

### Where it keeps things

| | |
|---|---|
| Settings | `HKCU\Software\SqeezeAmp\SqeezeAmp` |
| Password | Windows Credential Manager, under `SqeezeAmp/<host>:<port>` |
| Logs | `%LOCALAPPDATA%\SqeezeAmp\SqeezeAmp\logs\` — 2 MB × 5 generations |
| Artwork cache | `%LOCALAPPDATA%\SqeezeAmp\SqeezeAmp\cache\artwork\` — bounded, default 256 MB |

Closing the window keeps the player running in the tray. Quit from the tray
menu to actually stop it.

## The audio engine

SqeezeAmp does not decode anything itself. It supervises a **stock, unmodified
`squeezelite.exe`** as a child process and talks to it only through documented
command-line arguments and its log output. That binary is GPLv3 and is
deliberately **not** committed to this repository.

To get playback working:

1. Obtain a Windows build of squeezelite. Upstream is
   [ralph-irving/squeezelite](https://github.com/ralph-irving/squeezelite); its
   Windows builds are published at
   [sourceforge.net/projects/lmsclients](https://sourceforge.net/projects/lmsclients/files/squeezelite/windows/).
2. Put it at `packaging\windows\squeezelite.exe` (gitignored), or point
   `SQZ_ENGINE_EXE` at it.
3. Run `win-deploy.bat`, which stages it into `engine\` beside the application.

The version that was tested, with its checksum, is pinned in
[`packaging/engine-version.txt`](packaging/engine-version.txt). Treat an engine
upgrade as a change with a test pass behind it: `ExternalEngine::applyLogLine()`
scrapes a log format that upstream makes no promises about, and
`ExternalEngineTests` holds lines captured from the pinned build so a change
shows up as a red test rather than as an empty diagnostics panel.

Audio plays through the **shared** Windows mixer by design, so other
applications stay audible and Windows handles any sample-rate conversion. An
exclusive-mode toggle exists in Settings; turning it on silences every other
application on the PC.

## Package

```bat
win-deploy.bat                :: stage Qt, the audio engine and the licences
win-package.bat               :: portable zip + installer  → dist\
win-package.bat zip           :: portable zip only
```

`win-deploy.bat` runs `windeployqt` and copies the engine and the licence files
into the build output, which is also what makes the executable runnable from
Explorer. `win-package.bat` then builds:

- `dist\SqeezeAmp-<version>-windows-x64-portable.zip`
- `dist\SqeezeAmp-<version>-setup.exe`, if
  [Inno Setup 6](https://jrsoftware.org/isinfo.php) is installed — otherwise it
  builds the zip and says so.

Both come from the same staged tree, so what ships is what you ran.

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

Includes only ever point downward, and three ownership rules that the compiler
cannot express are enforced by `tools/check-layering.py`, which runs as a test.
[`CLAUDE.md`](CLAUDE.md) is the working guide: the traps, the invariants, and
where a new file goes.

## Licensing

Not settled yet — see [`prd.md`](prd.md) §11 and Q7. What *is* settled:

- The bundled `squeezelite.exe` is **GPLv3**, and any distributed build must
  carry its licence text and a written offer for its source. Both live in
  `packaging/licenses/`, and `CMakeLists.txt` refuses to configure without
  them. `WRITTEN-OFFER.txt` has two blanks — a distributor name and a contact —
  that a release must fill in.
- Qt is used under **LGPLv3**, which is why the Qt libraries ship as separate
  DLLs and are never linked statically: you must be able to relink against your
  own Qt build.
- A build that never leaves the machine that produced it owes none of this.
  GPLv3's obligations attach to distribution, not to use.

See [`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md) for the full picture,
including what is statically linked inside the engine binary.

# Contributing to KvitSqueeze

Bug reports, fixes and questions are welcome. Please read the first section
before opening a feature request — this project has a deliberately narrow scope,
and most of the obvious "why doesn't it…" answers are decisions rather than
omissions.

Contributions are accepted under the project's licence, [MPL-2.0](LICENSE).
There is no CLA. New source files should carry
`// SPDX-License-Identifier: MPL-2.0` as their first line.

---

## Settled decisions

These are design decisions, not gaps. Each was argued once and written down, and
re-opening one costs more than it looks like it should. A pull request against
any of them will not be merged without the discussion happening first — please
open an issue or a discussion instead, and say what changed.

### One player: itself

KvitSqueeze controls exactly one player — the one this process registers on the
server. There is no player list, no switcher, and no sync groups. Other players
are ignored even when they arrive in a server notification.

This is enforced, not merely intended. No player id may appear in the
application or QML layers; the session layer injects it into every request, and
a test reads the actual request bodies to prove none escapes with a different
one. "Let me pick which player to control" is a different product.

### My Music only

No plugins, no radio, no podcasts, no favourites, and no generic
`browselibrary` menu renderer. Browse screens are purpose-built and typed
against the classic commands.

**One named exception exists: Random Mix.** The rule is about *menu shape*, not
about the word "plugin". RandomPlay ships with the server, draws only from the
scanned library, and is six flat verbs with fixed arguments — so it is in.
Dynamic Playlists is a nested descriptor tree with prompting parameters, so it
is out.

If you want to propose another plugin, the useful question is not "why not
plugins" but **"which of those two shapes does it have?"** A flat, fixed set of
verbs over the local library is arguable. A tree the client has to render
generically is not.

### Local control only

The application exposes no network control surface. The only listening endpoint
in the process is a single-instance named pipe — never a TCP port, not even on
loopback. This is verified against a running build with `netstat`, and it is why
there is no web UI, no REST API and no companion app.

### Shared-mode audio is the product decision

Audio plays through the shared Windows mixer so other applications stay
audible, and the mixer's sample-rate conversion is accepted. This is a choice
about how the app should behave on a desktop, not a compromise pending a fix.
An exclusive-mode toggle exists, is off by default, and silences every other
application when enabled.

---

## Where a new file goes

`src/` is five libraries — `protocol`, `session`, `engine`, `app`, `qml` — and
includes may only point downward:

```
protocol   pure LMS protocol: requests, replies, the command vocabulary
session    the live connection; the only module that links Qt Network
engine     the audio engine behind IAudioEngine; the only module with QProcess
app        orchestration and the QML-facing models
qml        QML registrations, the composition root, Windows integration
```

A file in the wrong directory usually announces itself as `No such file or
directory` on an include that looks fine. Read that error as "wrong direction"
before assuming a typo.

The graph, plus three ownership rules the compiler cannot express, are enforced
by `tools/check-layering.py`, which runs as the `LayeringGuard` test:

- **Qt networking only in `sqz-session`.** Every request goes through
  `LmsSession`, which is also where the player id is injected.
- **`QProcess` only in `sqz-engine`.** The app drives exactly one child process
  — the audio engine — and nothing else.
- **No player id in `sqz-app` or `sqz-qml`.** `protocol` may take one as a
  parameter and `engine` passes it to squeezelite's `-m`, but a model or a QML
  binding carrying one is the first step back toward a player switcher.

The guard is line-based and does not know prose from code, so **a comment that
names `QTcpServer`, `QNetworkAccessManager` or `playerId` in the wrong module
fails the build too.** That is deliberate: reword the comment.

`engine` depends on `protocol` for exactly one type, `PlayerIdentity` — the
session and the engine both need this player's id, and no module that can see
both is allowed to carry it. Do not widen that dependency.

## The audio engine seam

`IAudioEngine` has exactly one implementation, `ExternalEngine`, which
supervises a stock `squeezelite.exe` child process. Two other backends are
specified and deliberately not built, and this interface is what keeps that
reversible:

- No `ExternalEngine`-specific type may appear above `src/engine/`.
- Nothing in `iaudioengine.h` may name `QProcess`, a command-line flag, or a log
  format.
- The engine knows nothing about LMS beyond an address.

`buildArguments()` and `applyLogLine()` are static pure functions precisely so
the entire interface to the child process — arguments out, status in — is
testable without launching anything.

**Never patch the squeezelite binary.** Talking to it only through documented
arguments and log output is what keeps KvitSqueeze's own licence unconstrained,
and patching it would silently change the log format the status scraper reads.
The binary is not shipped either — the installer downloads it from upstream —
which makes "stock and independently obtainable" a fact rather than a promise.

---

## Two rules that are easy to break by accident

**Unknown is not zero.** Engine status fields the backend cannot determine stay
at `-1` or empty, and the UI hides them. The backend can only scrape what
squeezelite happens to log, so a failed match must leave the field *unknown*
rather than guess — reporting a sample rate of 0 as fact is worse than reporting
nothing. The same applies to metadata: a duration of `-1` means "the server did
not say".

**The server wins.** Transport state, queue and volume belong to the server. The
UI may apply an optimistic local update, but it reconciles to the next
authoritative snapshot within 500 ms and the server wins any conflict. External
changes — the LMS web UI, a phone, a home-automation system — must appear
without a reload and without fighting local state.

---

## Building and testing

[`devel.md`](devel.md) is the full guide — toolchain, packaging, and the source
layout. In short:

```bat
win-build.bat            configure + build (Release)
win-build.bat build      build only
win-test.bat             full suite → win-test-result.txt
```

Things that will cost you an hour if nobody mentions them:

- **Read `win-test-result.txt`, not the console.** Piping ctest output through
  another process leaves stdout fully buffered, so output vanishes on a crash
  and phantom failures appear.
- **Close the running app before rebuilding.** A locked `kvitsqueeze.exe` fails
  its link with `LNK1104` while everything else succeeds, leaving a stale
  executable that looks freshly built.
- **`*.bat` files must keep CRLF line endings** (`.gitattributes` enforces it).
  A batch file saved with LF makes `cmd.exe` eat the first character of most
  lines and report a cascade of `'X' is not recognized`, which looks nothing
  like a line-ending problem.
- **`resources.qrc` is the only component list.** A QML file missing from it
  breaks the shipped shell and hangs a Qt Quick harness until its CTest timeout
  rather than failing with something readable.

Both test labels run by default:

- **`unit`** — deterministic. No display, no network, no child processes.
- **`shell`** — needs a QML engine and a window. Loads the real `Main.qml`
  through the real composition root with the session and engine switched off,
  and instantiates every view.

A test that talks to a real server, or launches a real `squeezelite.exe`,
belongs to neither: it is a manual check, not a suite entry.

## QML notes

- **`Theme.qml` is not a singleton, on purpose.** A QML singleton needs a
  `qmldir` entry, and `resources.qrc` is the only component list here; a file
  that must appear in two places will eventually appear in one. Write
  `Theme { id: theme }` in each file that needs it — every instance follows the
  same settings object, so they cannot drift.
- **Never name a property `onSomething`.** QML parses a name starting with `on`
  plus a capital as a signal handler and rejects the file with "Cannot assign a
  value to a signal", which says nothing about the real cause.
- **Reading a model row outside a delegate goes through `get(row)`**, which
  returns a map keyed by role name. Arithmetic on `Qt.UserRole` in QML breaks
  silently the first time a role is inserted into the middle of an enum.
- Types QML needs are registered as `QML_FOREIGN` wrappers in
  `src/qml/qmlsingletons.h`, **never with a macro on the class itself**.
  `qmltyperegistrar` reads one target's metatypes, so a macro anywhere else is
  silently ignored and QML fails at runtime with
  `ReferenceError: <Type> is not defined`.
- `qt_add_qml_module` generates a registration object file that nothing
  references, so a static library link drops it and the `Sqz` module ends up
  empty. `import Sqz` still succeeds, and then every use of a Sqz name fails at
  *runtime*. `sqz_keep_qml_registration()` in `CMakeLists.txt` forces the object
  file in; call it for any new binary that loads QML.

---

## A note on the `FR-` and `N` references in comments

Source comments cite requirement ids — `FR-2.5`, `NFR-10`, `N5`, `D15`. These
come from the project's specification and progress log, which are kept as
internal working documents and are not published: they carry network and library
detail specific to the machine this was developed on.

The ids are stable, and they are there so a future change can be traced to the
decision that shaped it. Every invariant a contributor actually needs to respect
is restated in this file — if you hit a citation whose reasoning is not obvious
from the surrounding code and not covered here, that is a fair thing to ask
about in an issue, and a good sign this document is missing something.

## Reporting a bug

Include the KvitSqueeze version, your Lyrion Music Server version, and **the
output of the copy button on the Diagnostics screen** (Settings →
Diagnostics). That button exists so a bug report can carry the engine's actual
state — what it negotiated, what it printed, and what it could not determine.

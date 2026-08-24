# SqeezeAmp

[![CI](https://github.com/kvit-s/sqeezeamp/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/kvit-s/sqeezeamp/actions/workflows/ci.yml)

A native Windows player for [Lyrion Music Server](https://lyrion.org) (LMS,
formerly Logitech Media Server). One Qt 6 / C++20 application that registers
itself as a SqueezeBox player, browses your library, and plays to a local audio
device — with a real desktop UI rather than the server's web skin in a browser
control.

![SqeezeAmp playing an album, showing the Now Playing view](docs/now-playing.png)

**It controls exactly one player: itself.** Other players on your server —
hardware Squeezeboxes, other squeezelite instances, bridges — are not listed,
not selectable, and not controllable. This is a music player for this PC, not a
remote control for the house.

**Status: a working beta.** It browses, queues and plays against a real server,
and the protocol, reconciliation and engine seams are covered by tests. Most of
the Windows integration — tray, media keys, SMTC, taskbar buttons — is built but
has not been exercised by hand, and the long-running behaviour (a 12-hour soak,
a server restart, a DAC unplugged mid-track) has code behind it but no evidence.

## Why this instead of Squeezelite-X?

Squeezelite-X is the tool this was written to replace, and it is worth being
precise about which half of it. It is a closed-source Windows shell that bundles
the same `squeezelite` audio engine SqeezeAmp drives, and then hosts the
server's own web UI — Material Skin — inside an embedded browser control.

**The audio half of that stack is genuinely good.** squeezelite is mature,
gapless, and has had a decade of field testing. SqeezeAmp does not try to
improve on it; it drives the same binary. The UI half is the part worth
replacing.

| | Squeezelite-X | SqeezeAmp |
|---|---|---|
| The interface | The server's web page in an embedded browser, with the latency and scroll feel that implies | Native Qt 6 / QML, drawn by the application |
| Source | Closed — you cannot fix it, theme it, or contribute to it | MPL-2.0, all of it, including the UI |
| Player and interface | Two loosely coupled pieces, so device changes, volume and player state do not always behave as one app | One process — transport, volume and output device are the same program |
| Windows integration | Media keys, tray, taskbar and per-app volume behave inconsistently | Built as first-class: tray, SMTC, media keys, taskbar buttons, per-app volume |
| Scope | Everything the web UI can reach — plugins, radio, podcasts, favourites | My Music only, deliberately |

**That last row is a trade, not a boast.** If you live in radio, podcasts or
plugin menus, Squeezelite-X reaches things SqeezeAmp will not.

**Scope, in one paragraph.** My Music only: no plugins, no radio, no podcasts,
no favourites — with one deliberate exception, the server's own Random Mix. One
player, itself, with no switcher and no sync groups. No network control surface
of any kind. Audio through the shared Windows mixer so everything else stays
audible. These are decisions rather than gaps, and
[`CONTRIBUTING.md`](CONTRIBUTING.md) explains each one and what would have to
change to reopen it.

---

## Install

You need 64-bit Windows 10 or 11, and an LMS somewhere on your network. Nothing
else — no Qt, no Visual Studio, no separate squeezelite download.

### 1. Download

Go to the [**latest release**](https://github.com/kvit-s/sqeezeamp/releases/latest)
and take one of:

| | |
|---|---|
| `SqeezeAmp-<version>-setup.exe` | The installer. Defaults to a per-user install, which needs **no administrator rights**. |
| `SqeezeAmp-<version>-windows-x64.zip` | Portable. Unpack it anywhere and run it. See [below](#or-the-portable-zip). |
| `SHA256SUMS-windows.txt` | Checksums for both, if you want to verify what you downloaded. |

To check the download — worth doing, and see the next section for why:

```powershell
Get-FileHash .\SqeezeAmp-0.1.0-setup.exe -Algorithm SHA256
```

Compare the result with the line in `SHA256SUMS-windows.txt`.

### 2. Windows will warn you, and here is why

**SqeezeAmp is not code-signed.** A code-signing certificate costs a few
hundred dollars a year, and this is a beta nobody is charging for. That is the
whole reason, and it means you will see up to three warnings. None of them
means anything is wrong; all of them mean the same thing, which is that Windows
does not recognise the publisher.

**Your browser, at download time.** Edge and Chrome flag executables that few
people have downloaded. Edge says *"…isn't commonly downloaded. Make sure you
trust…"* — click the **…** next to the download and choose **Keep**, then
**Show more** → **Keep anyway** if it asks again.

**SmartScreen, when you run it.** A blue full-window dialog: *"Windows
protected your PC — Microsoft Defender SmartScreen prevented an unrecognised
app from starting."* There is no visible Run button. Click **More info**, which
reveals **Run anyway**.

**Your antivirus, possibly.** A small, brand-new, unsigned installer that
downloads a second executable is a shape heuristics dislike. If it is
quarantined, the checksum above is the real integrity check — it is stronger
evidence than any of these warnings, because it is the one thing an attacker
who tampered with the file could not reproduce.

If none of that sits comfortably with you, [build it
yourself](devel.md) — the source here is the whole application.

### 3. Run the installer

The wizard is short:

1. **Install mode**, if it is offered — *Install for me only* is what SqeezeAmp
   asks for and needs no administrator rights. *Install for all users* will
   raise a UAC prompt; nothing here needs it.
2. **Licence** — MPL-2.0, SqeezeAmp's own.
3. **Destination** — defaults to your per-user programs folder.
4. **Start Menu folder.**
5. **Tasks** — three checkboxes:
   - *Download the audio engine (squeezelite)* — **ticked by default, and you
     want it.** See [The audio engine](#the-audio-engine) for what this is and
     why it is a download rather than something in the installer.
   - *Create a desktop shortcut* — off by default.
   - *Start SqeezeAmp with Windows, minimised to the tray* — off by default.
6. **Install** — this is when the engine is fetched, about 2.7 MB, so setup
   needs a working internet connection at this point. It is verified against a
   pinned checksum before it is used.
7. **Finish** — offers to launch the app.

**If the engine download fails**, setup still completes and tells you so. The
app installs and runs; it will report that no engine is present instead of
playing. Run `fetch-engine.ps1` from the installation folder once you have a
connection, and it will do the same job:

```powershell
powershell -ExecutionPolicy Bypass -File fetch-engine.ps1
```

To uninstall, use **Settings → Apps** or the Start Menu shortcut. Your settings
and the player's identity are kept on purpose, so reinstalling gets the same
player back with its server-side queue intact.

### Or the portable zip

Windows marks downloaded archives, and that mark is inherited by everything
extracted from them — which makes the app and its scripts fight you. Clear it
on the zip **before** extracting:

right-click the `.zip` → **Properties** → tick **Unblock** → **OK**

Then extract it anywhere, and from the extracted folder:

```powershell
powershell -ExecutionPolicy Bypass -File fetch-engine.ps1
```

That downloads the audio engine into `engine\` beside the application — the
zip does not carry one, for the reasons in [The audio
engine](#the-audio-engine). Then run `sqeezeamp.exe`.

---

## Using it

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
**not** a socket, which is what keeps the no-remote-control rule intact. Two
consequences worth knowing:

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

### Lyrics

Click the cover on Now Playing. The server serves whatever plain lyric tag the
file carries, which is enough to read but not to follow.

For lyrics that follow the song, point **Settings → Music folder on this PC**
at the same music the server is playing — a mapped drive, a UNC share, a local
folder. SqeezeAmp then looks for an `.lrc` beside each track and highlights the
line being sung. Lyrion does not read those files itself, which is why this is
the client's job and why the setting exists at all.

Left empty, nothing is read from disk and the pane shows the server's copy.
A sheet with no timings is drawn with **no line highlighted**: where the file
does not say, the app does not guess.

### Where it keeps things

| | |
|---|---|
| Settings | `HKCU\Software\SqeezeAmp\SqeezeAmp` |
| Password | Windows Credential Manager, under `SqeezeAmp/<host>:<port>` |
| Logs | `%LOCALAPPDATA%\SqeezeAmp\SqeezeAmp\logs\` — 2 MB × 5 generations |
| Artwork cache | `%LOCALAPPDATA%\SqeezeAmp\SqeezeAmp\cache\artwork\` — bounded, default 256 MB |

Closing the window keeps the player running in the tray. Quit from the tray
menu to actually stop it.

---

## The audio engine

SqeezeAmp does not decode anything itself. It supervises a **stock, unmodified
`squeezelite.exe`** as a child process and talks to it only through documented
command-line arguments and its log output. That is the same engine
Squeezelite-X uses, and it is the part of the stack there was never any reason
to replace.

**That binary is GPLv3, and SqeezeAmp does not distribute it.** Neither the
installer nor the portable zip contains it: the installer fetches it from
upstream during setup, and the zip carries `fetch-engine.ps1`, which does the
same when you run it. Both verify it against a pinned SHA-256 before use.

**Where it is downloaded from is not baked into the installer.** It comes from
[`packaging/engine-manifest.txt`](packaging/engine-manifest.txt), read over the
network from this repository at install time, because upstream keeps only a
rolling window of Windows builds on
[SourceForge](https://sourceforge.net/projects/lmsclients/files/squeezelite/windows/)
and prunes the rest — the build this project first pinned is already gone from
there. That means an installer you downloaded a year ago can be repaired by
updating one file here, instead of being permanently broken by a URL that went
away.

The build that was tested, with its checksum, is pinned in
[`packaging/engine-version.txt`](packaging/engine-version.txt).

Audio plays through the **shared** Windows mixer by design, so other
applications stay audible and Windows handles any sample-rate conversion. An
exclusive-mode toggle exists in Settings; turning it on silences every other
application on the PC.

## Building from source

See [**`devel.md`**](devel.md) — toolchain, build, test, packaging and the
source layout. [`CONTRIBUTING.md`](CONTRIBUTING.md) is the working guide: the
settled scope decisions, the module boundary, the invariants and the traps.

## Licensing

**SqeezeAmp is [MPL-2.0](LICENSE).** The licence covers every file in this
repository; source files carry an `SPDX-License-Identifier: MPL-2.0` line, and
MPL Exhibit A's `LICENSE`-file fallback covers the few that do not (the build
scripts, the packaging inputs, and the documents).

MPL is file-level copyleft: a Larger Work built around these files can carry
whatever terms you like, but modifications to *these* files stay under MPL. That
is deliberate. The complaint this project was started over is a player UI
nobody can fix, extend or theme; this is the licence that makes sure this one
never becomes that.

Exhibit B, the "Incompatible With Secondary Licenses" notice, is **not** used
anywhere and must not be added. Without it, MPL-2.0 §3.3 lets these files also
be distributed under a Secondary License — GPLv2-or-later, LGPLv2.1-or-later or
AGPLv3 — when combined with a work governed by one. That is what keeps an
otherwise awkward question academic rather than risky: whether supervising a
GPLv3 `squeezelite.exe` as a child process makes one work or two has no settled
answer, and if it were ever held to be one work, the combination can simply be
distributed under GPLv3.

The rest:

- The `squeezelite.exe` SqeezeAmp drives is **GPLv3**, and SqeezeAmp does not
  distribute it — the installer fetches it from upstream, so there is no
  corresponding source to attach to a release and no written offer to make.
  Its licence text ships anyway (`packaging/licenses/`, and `CMakeLists.txt`
  refuses to configure without it), so a user whose installer fetched the
  engine has its terms on disk.
- Qt is used under **LGPLv3**, which is why the Qt libraries ship as separate
  DLLs and are never linked statically: you must be able to relink against your
  own Qt build.
- A build that never leaves the machine that produced it owes none of this.
  GPLv3's obligations attach to distribution, not to use.

See [`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md) for the full picture,
including what is statically linked inside the engine binary.

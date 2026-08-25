# KvitSqueeze

[![Latest release](https://img.shields.io/github/v/release/kvit-s/kvitsqueeze?display_name=tag&label=download)](https://github.com/kvit-s/kvitsqueeze/releases/latest)
[![CI](https://github.com/kvit-s/kvitsqueeze/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/kvit-s/kvitsqueeze/actions/workflows/ci.yml)

A native Windows music player for [Lyrion Music Server](https://lyrion.org)
(LMS). It registers itself as a player on your server, browses your library, and
plays to this PC's audio device.

![KvitSqueeze playing an album, showing the Now Playing view](docs/now-playing.png)

## Why it exists

Most Windows clients for LMS are shells around the server's web page. This one
was written for five things:

- A player, not a remote. It controls one player: itself.
- Control over its own playback rather than reacting to server push.
- Random mixes as a first-class feature.
- Working Windows media keys.
- Pausing itself while the microphone is in use.

## Random mixes

Five types — **Song, Album, Artist, Year, Work** — from the mix button in the
bottom bar, on every screen.

- <kbd>Ctrl</kbd>+<kbd>R</kbd> starts a Song Mix, <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> stops the running one.
- Start, re-roll and stop are also in the tray menu.
- **Choose genres…** narrows the pool and stays narrowed until changed.
- Starting a mix replaces the queue, so it asks first — unless the queue is
  empty or already a mix.
- The queue header names the mix and counts what is queued:
  `Song Mix · 10 ahead · a moving window`. The mix button lights, the running
  type is ticked in the menu, and the tray tooltip carries a line.
- A mix that stops on its own produces a tray notification and a log entry. Mix
  state is polled rather than pushed, and shows as unknown until the server
  answers.

## Windows integration

- Four media keys, global, with or without window focus.
- SMTC: the Windows now-playing overlay, with track and cover.
- Taskbar thumbnail buttons: previous, play/pause, next. Glyphs recolour with
  the Windows light/dark setting.
- Tray icon with the current track in its tooltip, transport menu and mix
  controls.
- Optional start with Windows, minimised to the tray.

For a keyboard with no media keys, see [Scripting](#scripting).

## Microphone pause

**Settings → Pause while the microphone is in use.** Off by default.

Any application opening the microphone pauses playback — voice typing
(<kbd>Win</kbd>+<kbd>H</kbd>), a call, a meeting. Playback resumes after a delay
you set (**Wait before resuming**, default 3 s), and only if nothing else has
touched the player meanwhile. Pause it yourself and it stays paused.

## Lyrics

Click the cover on Now Playing. The line being sung is highlighted, and the view
scrolls to keep it visible.

The server serves only the plain lyric tag in the file, which has no timings.
For timed lyrics, point **Settings → Music folder on this PC** at the same files
the server plays — mapped drive, UNC share or local folder. KvitSqueeze reads the
`.lrc` sidecar next to each track. Lyrion does not read those files.

Lyrics without timings display with no line highlighted.

## Playback

- **32 MB stream buffer.** A track transfers in a second or two, so pausing does
  not leave a partly-transferred stream open.
- **Five resampling presets.** Resamples only when the device cannot take the
  source rate.
- **Shared Windows mixer by default**, so other applications stay audible. An
  exclusive-mode toggle is available and silences everything else.
- **Output device stored by name**, not index, so a hot-plug cannot repoint it.
- **Diagnostics panel**: decoder, source and output sample rate, device,
  underruns. Values the engine does not report show as unknown, not zero.
- A device that will not open at the track's rate is reported by name with the
  setting that fixes it; otherwise it is indistinguishable from playing.

## Browsing and queue

- My Music: artists, albums, genres, years, playlists, folders, search. No
  plugins, radio or podcasts.
- Album art cached on disk, 256 MB by default. Long lists page as you scroll.
- Queue: drag to reorder, remove, clear, save as playlist. Shuffle and repeat
  follow the server's state.
- External changes — LMS web UI, a phone, Home Assistant — appear without a
  reload. Where local and server state disagree, the server wins.
- Other players on the server are not listed, selectable or controllable.

---

## Install

64-bit Windows 10 or 11, and an LMS on your network.

### 1. Download

From the [latest release](https://github.com/kvit-s/kvitsqueeze/releases/latest):

| | |
|---|---|
| `KvitSqueeze-<version>-setup.exe` | Installer. Per-user by default, no administrator rights. |
| `KvitSqueeze-<version>-windows-x64.zip` | Portable. See [below](#or-the-portable-zip). |
| `SHA256SUMS-windows.txt` | Checksums for both. |

```powershell
Get-FileHash .\KvitSqueeze-0.1.0-setup.exe -Algorithm SHA256
```

### 2. Warnings you will see

KvitSqueeze is not code-signed, so Windows does not recognise the publisher.

- **Browser:** Edge and Chrome flag rarely-downloaded executables. Choose
  **Keep**, then **Show more → Keep anyway**.
- **SmartScreen:** *"Windows protected your PC"*, with no visible Run button.
  Click **More info → Run anyway**.
- **Antivirus:** an unsigned installer that downloads a second executable may be
  flagged.

Verify the checksum above if you want confirmation, or
[build it yourself](devel.md).

### 3. Run the installer

1. **Install mode**, if offered — per-user needs no administrator rights.
2. Licence, destination, Start Menu folder.
3. **Tasks** — *Download the audio engine* is on by default and required for
   playback. Desktop shortcut and start-with-Windows are off.
4. **Install** — the engine is fetched here, about 2.7 MB, checked against a
   pinned checksum. Setup needs an internet connection at this point.
5. **Finish.**

If the download fails, setup still completes and says so — and the app offers to
do it itself the first time you run it, with a **Download the audio engine**
button. `fetch-engine.ps1` in the installation folder is still there for a shell:

```powershell
powershell -ExecutionPolicy Bypass -File fetch-engine.ps1
```

Uninstall from **Settings → Apps**. Settings and the player identity are kept, so
reinstalling gets the same player and its server-side queue back.

### Or the portable zip

Windows marks downloaded archives and everything extracted from them inherits
the mark. Clear it first: right-click the zip → **Properties** → **Unblock**.
Then extract and run `kvitsqueeze.exe`.

The zip carries no audio engine — squeezelite is GPLv3 and this project does not
distribute it, see [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md). The app says
so on its first run and offers a **Download the audio engine** button; that is
all there is to it, and nothing needs restarting afterwards. If a firewall
blocks the download, the same panel takes a `squeezelite.exe` you already have.

From a shell instead, before or after first run:

```powershell
powershell -ExecutionPolicy Bypass -File fetch-engine.ps1
```

---

## Using it

### First run

The app starts with no server configured. Open **Settings** and enter the host
and port (default `9000`).

Discovery broadcasts on UDP 3483, which does not cross a router, so a server on
another subnet — including any LMS running as a Home Assistant add-on — has to
be typed in. Auth is optional; passwords go to the Windows Credential Manager.

The app registers as a player under the name in **Settings → Player** (default
`KvitSqueeze (<machine>)`) and keeps that identity across restarts, so its queue
and per-player settings persist on the server.

### Keyboard

| | |
|---|---|
| <kbd>Space</kbd> | Play / pause |
| <kbd>←</kbd> <kbd>→</kbd> | Seek ∓5 s |
| <kbd>Ctrl</kbd>+<kbd>←</kbd> <kbd>→</kbd> | Previous / next track |
| <kbd>↑</kbd> <kbd>↓</kbd> | Volume |
| <kbd>Ctrl</kbd>+<kbd>R</kbd> / <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | Start Song Mix / stop mix |
| <kbd>Ctrl</kbd>+<kbd>F</kbd> | Search |
| <kbd>Ctrl</kbd>+<kbd>U</kbd> | Queue |
| <kbd>Ctrl</kbd>+<kbd>M</kbd> | Mini player |
| <kbd>Ctrl</kbd>+<kbd>,</kbd> | Settings |
| <kbd>Esc</kbd> | Close lyrics, or go back |

### Command line

```
kvitsqueeze.exe --minimized     start hidden in the system tray
kvitsqueeze.exe --play-pause    send a transport command to the running copy
kvitsqueeze.exe --next          and exit; does nothing if none is running
kvitsqueeze.exe --previous
kvitsqueeze.exe --stop
```

A second launch raises the existing window instead of starting a second copy.
`--help` and `--version` answer in a message box, since this is a GUI binary
with no console.

### Scripting

`tools/sqz-remote.au3` sends verbs to the running app, avoiding Qt start-up per
keypress:

```
sqz-remote.au3 next          also: previous, playpause, stop, activate
```

Both routes use a per-user named pipe that accepts those five verbs and nothing
else. It is not a network socket. Unlike a media key, which the player that
grabbed the hotkey first answers, this reaches this app specifically.

### Files and settings

| | |
|---|---|
| Settings | `HKCU\Software\KvitSqueeze\KvitSqueeze` |
| Password | Windows Credential Manager, `KvitSqueeze/<host>:<port>` |
| Logs | `%LOCALAPPDATA%\KvitSqueeze\KvitSqueeze\logs\` — 2 MB × 5 |
| Artwork cache | `%LOCALAPPDATA%\KvitSqueeze\KvitSqueeze\cache\artwork\` — 256 MB default |

Closing the window ends the app. **Settings → Closing the window keeps playing in
the tray** changes that.

## Audio engine

KvitSqueeze does not decode audio. It drives a stock, unmodified `squeezelite` as
a supervised child process.

That binary is GPLv3, so it is not shipped here. It is downloaded from upstream
and checked against a pinned checksum — by the installer during setup, by the app
itself from a button the first time it finds none, or by `fetch-engine.ps1` from
a shell. This is why setup needs an internet connection.

The app looks for it at `engine\squeezelite.exe` beside the executable, falling
back to `%LOCALAPPDATA%\KvitSqueeze\KvitSqueeze\engine\` when the installation
folder is not writable. One appearing in either place is picked up while the app
is running, so there is nothing to restart.

## Status

0.1.0, a beta. In daily use on the author's machine.
[Issues](https://github.com/kvit-s/kvitsqueeze/issues).

## Building from source

[`devel.md`](devel.md) — toolchain, build, test, packaging, layout.
[`CONTRIBUTING.md`](CONTRIBUTING.md) — scope decisions and invariants.

## Licence

KvitSqueeze is [MPL-2.0](LICENSE), including the UI. The audio engine it drives is
GPLv3 and is downloaded rather than distributed here. Qt is used under LGPLv3.
See [`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md).

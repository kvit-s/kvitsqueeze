# SqeezeAmp

[![CI](https://github.com/kvit-s/sqeezeamp/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/kvit-s/sqeezeamp/actions/workflows/ci.yml)

A native Windows music player for [Lyrion Music Server](https://lyrion.org)
(LMS, formerly Logitech Media Server). It registers itself as a player on your
server, browses your library, and plays to this PC's audio device.

![SqeezeAmp playing an album, showing the Now Playing view](docs/now-playing.png)

## Why it exists

Most Windows clients for LMS are thin shells around the server's own web page:
the server pushes, the app displays. That is fine until you want the player
itself to have an opinion — about how much it has buffered, about what plays
next, about how any of it looks.

SqeezeAmp was written for five things in particular:

- **It is a player, not a remote.** It controls exactly one player: itself.
- **It controls its own playback**, rather than reacting to whatever the server
  pushed at it.
- **Random mixes are a first-class feature**, not a plugin menu three levels
  deep.
- **The Windows media keys work**, along with the rest of the desktop shell.
- **It gets out of the way when you talk** — dictation, calls, meetings.

The rest of this section is what each of those turned into.

---

## Random mixes

Five kinds, from a menu on the mix button in the bottom bar, reachable from
every screen:

**Song Mix · Album Mix · Artist Mix · Year Mix · Work Mix**

<kbd>Ctrl</kbd>+<kbd>R</kbd> starts a Song Mix, <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd>
stops whatever is running. Start, re-roll and stop are also in the tray menu,
so you never have to open the window to change your mind.

**Choose genres…** narrows the pool, and the narrowing sticks until you widen
it again.

Because a mix replaces the queue, SqeezeAmp asks first — but only when there is
something to lose, meaning the queue is neither empty nor already a mix.

**It tells you a mix is running, and how much of it is ahead.** The queue header
names it in words and counts what is queued — `Song Mix · 10 ahead · a moving
window` — because a random mix is not a playlist with an end, and a queue that
looks ten tracks long is misleading unless it says so. The mix button lights up,
the mix menu ticks the running type, and the tray tooltip carries a line.

**And it tells you when a mix stops on its own**, with a tray notification and a
line in the log. The server's random-play plugin announces nothing when a mix
starts or ends, so SqeezeAmp asks it — on connect, after anything that touched
the queue, and once a heartbeat. Until it has an answer the state is shown as
*unknown* rather than guessed at.

## Windows media keys, and the rest of the shell

- **The four media keys work globally**, whether or not the window has focus.
- **SMTC** — the Windows now-playing overlay that appears when you press a media
  key, with the track and cover.
- **Taskbar thumbnail buttons** — previous, play/pause, next on the taskbar
  preview. The glyphs are drawn rather than shipped, so they recolour when
  Windows switches between light and dark.
- **Tray icon** with the current track in its tooltip, a transport menu, and the
  random-mix controls.
- **Start with Windows**, minimised to the tray, if you want it.

**For a keyboard with no media keys** — a Microsoft Sculpt, say — any remappable
key can drive the running app instead. See
[Driving it from a script](#driving-it-from-a-script).

## It pauses itself when you talk

**Settings → Pause while the microphone is in use.** Off by default.

Any application that opens the microphone pauses playback: Windows voice typing
(<kbd>Win</kbd>+<kbd>H</kbd>), a call, a meeting. When the microphone is
released, playback resumes after a delay you set — **Wait before resuming**,
three seconds by default — and only if nothing else has touched the player in the
meantime. Pause it yourself during a meeting and it stays paused.

Three seconds is measured rather than chosen: voice typing dismisses its own
panel after a silence, and pausing to think before pressing
<kbd>Win</kbd>+<kbd>H</kbd> again produces a close/open pair about 3.1 s apart.
Any shorter and a bar of music arrives in the middle of your sentence.

## Lyrics that follow the song

Click the cover on Now Playing. The sheet opens over the pane, one line per row,
the line being sung in white and the rest dimmed, scrolling to keep it in view.

The server only serves whatever plain lyric tag a file carries, which is enough
to read but not to follow. **For lyrics that follow**, point **Settings → Music
folder on this PC** at the same music the server is playing — a mapped drive, a
UNC share, a local folder. SqeezeAmp then finds the `.lrc` file sitting beside
each track and uses its timings. Lyrion does not read those files at all, which
is why this is the client's job.

A sheet with no timings is drawn with **no line highlighted**. Spacing the lines
evenly across the track would be a guess presented as the line you are hearing.

## Playback it actually controls

**A whole track is pulled across before it needs to be.** The stream buffer is
32 MB, far above the stock default, which means an average track is on this PC
in a second or two. Pause for twenty minutes and the stream is not sitting there
half-transferred waiting to be dropped by something on the network — the file is
already here.

**Resampling has five presets** and only resamples when the output device cannot
take the track's own rate.

**Audio goes through the shared Windows mixer by design**, so everything else on
the PC stays audible. An exclusive-mode toggle exists in Settings and says
plainly that turning it on silences every other application.

**Output devices are remembered by name**, not by index, so plugging in a headset
cannot silently repoint playback at the TV.

**And when the output fails, it says so.** A device that will not open at the
track's rate is the one failure indistinguishable from playing — the server
streams, the transport says play, the position advances, and there is no sound.
SqeezeAmp names it and tells you which setting fixes it. There is a Diagnostics
panel with the decoder, sample rates, device and underrun counts; anything the
engine does not report is shown as unknown rather than as zero.

## Browsing and the queue

My Music, browsed as typed screens rather than a generic server menu: artists,
albums, genres, years, playlists, folders, and search. Album art is cached on
disk. Long lists page as you scroll — a 2,300-row artist list stays smooth.

The queue shows what is coming, highlights the current track, follows it while
it moves, and supports drag to reorder, remove, clear, and save-as-playlist.
Shuffle and repeat follow the server's own state.

**Everything reconciles to the server.** Change the volume from your phone, the
LMS web UI or Home Assistant and it shows up here without a reload. Where local
and server state disagree, the server wins.

**One player: itself.** Other players on your server — hardware Squeezeboxes,
other squeezelite instances, bridges — are not listed, not selectable, and not
controllable. This is a music player for this PC, not a remote control for the
house. No plugins, no radio, no podcasts: My Music, plus the random mixes above.

---

## Install

You need 64-bit Windows 10 or 11, and an LMS somewhere on your network.

### 1. Download

Go to the [**latest release**](https://github.com/kvit-s/sqeezeamp/releases/latest)
and take one of:

| | |
|---|---|
| `SqeezeAmp-<version>-setup.exe` | The installer. Defaults to a per-user install, which needs **no administrator rights**. |
| `SqeezeAmp-<version>-windows-x64.zip` | Portable. Unpack it anywhere and run it. See [below](#or-the-portable-zip). |
| `SHA256SUMS-windows.txt` | Checksums for both, if you want to verify what you downloaded. |

```powershell
Get-FileHash .\SqeezeAmp-0.1.0-setup.exe -Algorithm SHA256
```

Compare the result with the line in `SHA256SUMS-windows.txt`.

### 2. Windows will warn you, and here is why

**SqeezeAmp is not code-signed.** A certificate costs a few hundred dollars a
year and nobody is charging for this. That is the whole reason, and it means you
may see up to three warnings, all of which mean the same thing: Windows does not
recognise the publisher.

- **Your browser, at download time.** Edge and Chrome flag executables few people
  have downloaded. Click the **…** next to the download and choose **Keep**, then
  **Show more** → **Keep anyway** if it asks again.
- **SmartScreen, when you run it.** A blue dialog: *"Windows protected your PC."*
  There is no visible Run button — click **More info**, which reveals **Run
  anyway**.
- **Your antivirus, possibly.** A small, brand-new, unsigned installer that
  downloads a second executable is a shape heuristics dislike.

The checksum above is the check that carries real evidence. If none of this sits
comfortably, [build it yourself](devel.md) — the source here is the whole
application.

### 3. Run the installer

1. **Install mode**, if offered — *Install for me only* is what SqeezeAmp asks
   for and needs no administrator rights.
2. **Licence**, **Destination**, **Start Menu folder** — all unremarkable.
3. **Tasks** — *Download the audio engine* is **ticked by default and you want
   it** (see [The audio engine](#the-audio-engine)). Desktop shortcut and start
   with Windows are off by default.
4. **Install** — this is when the engine is fetched, about 2.7 MB, so setup needs
   a working internet connection at this point. It is checked against a pinned
   checksum before use.
5. **Finish** — offers to launch the app.

**If that download fails**, setup still completes and says so. The app installs
and runs; it reports that no engine is present instead of playing. Run
`fetch-engine.ps1` from the installation folder once you have a connection:

```powershell
powershell -ExecutionPolicy Bypass -File fetch-engine.ps1
```

Uninstall from **Settings → Apps** or the Start Menu. Your settings and the
player's identity are kept on purpose, so reinstalling gets the same player back
with its server-side queue intact.

### Or the portable zip

Windows marks downloaded archives, and everything extracted inherits the mark.
Clear it on the zip **before** extracting: right-click → **Properties** → tick
**Unblock** → **OK**. Then extract, and from the extracted folder:

```powershell
powershell -ExecutionPolicy Bypass -File fetch-engine.ps1
```

That fetches the audio engine into `engine\` beside the application. Then run
`sqeezeamp.exe`.

---

## Using it

### First run

The app starts with **no server configured** and says so in a banner under the
title bar. Open **Settings** and type your server's host and port (default
`9000`).

**Discovery will probably find nothing, and that is normal.** It broadcasts on
UDP 3483, and a broadcast does not cross a router — so a server on another
subnet, which includes any LMS running as a Home Assistant add-on, has to be
typed in. Auth is optional; if your server needs it, the password goes to the
Windows Credential Manager and never into the settings.

Once connected the app registers itself as a player, appearing in your server's
player list under the name in **Settings → Player** (default
`SqeezeAmp (<your-machine>)`). It keeps that identity across restarts, so its
queue and per-player settings survive on the server.

### Keyboard

| | |
|---|---|
| <kbd>Space</kbd> | Play / pause |
| <kbd>←</kbd> <kbd>→</kbd> | Seek ∓5 s |
| <kbd>Ctrl</kbd>+<kbd>←</kbd> <kbd>→</kbd> | Previous / next track |
| <kbd>↑</kbd> <kbd>↓</kbd> | Volume |
| <kbd>Ctrl</kbd>+<kbd>R</kbd> / <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | Start a Song Mix / stop the mix |
| <kbd>Ctrl</kbd>+<kbd>F</kbd> | Search |
| <kbd>Ctrl</kbd>+<kbd>U</kbd> | Queue |
| <kbd>Ctrl</kbd>+<kbd>M</kbd> | Mini player |
| <kbd>Ctrl</kbd>+<kbd>,</kbd> | Settings |
| <kbd>Esc</kbd> | Close the lyrics, or go back |

### Command line

```
sqeezeamp.exe --minimized     start hidden in the system tray

sqeezeamp.exe --play-pause    hand a transport command to the running copy
sqeezeamp.exe --next          and exit; does nothing if none is running
sqeezeamp.exe --previous
sqeezeamp.exe --stop
```

A second launch does not start a second copy: it raises the window of the one
already running and exits. `--help` and `--version` answer in a **message box** —
this is a GUI binary with no console attached, so Qt has nowhere else to put
them.

### Driving it from a script

Launching a process per keypress costs a few hundred milliseconds of Qt
start-up, so `tools/sqz-remote.au3` talks to the running app directly:

```
sqz-remote.au3 next          also: previous, playpause, stop, activate
```

Both routes end at the same place — a per-user named pipe that accepts those
five verbs and ignores everything else. It is the app's only listening endpoint
and it is **not** a network socket, which is what keeps the no-remote-control
rule intact. Unlike a media key, which whichever player grabbed the hotkey first
will answer, this reaches **this** app specifically.

### Where it keeps things

| | |
|---|---|
| Settings | `HKCU\Software\SqeezeAmp\SqeezeAmp` |
| Password | Windows Credential Manager, under `SqeezeAmp/<host>:<port>` |
| Logs | `%LOCALAPPDATA%\SqeezeAmp\SqeezeAmp\logs\` — 2 MB × 5 generations |
| Artwork cache | `%LOCALAPPDATA%\SqeezeAmp\SqeezeAmp\cache\artwork\` — bounded, default 256 MB |

Closing the window ends the app by default. **Settings → Closing the window keeps
playing in the tray** changes that; quit from the tray menu to stop it.

## The audio engine

SqeezeAmp does not decode audio itself. It drives a stock, unmodified
`squeezelite` — the same mature engine most LMS players on Windows use — as a
child process it supervises, restarts and reports on. There was never a good
reason to rewrite that part; the parts above it are what this project is.

That binary is GPLv3, so **SqeezeAmp does not ship it**: the installer downloads
it from upstream during setup and checks it against a pinned checksum, and the
portable zip carries the script that does the same. This is also why setup needs
an internet connection, and why a failed download is not a failed install.

## Status

**0.1.0 — a beta.** In daily use on the author's machine. Some corners are newer
and better travelled than others; if something misbehaves,
[open an issue](https://github.com/kvit-s/sqeezeamp/issues).

## Building from source

See [**`devel.md`**](devel.md) — toolchain, build, test, packaging and layout.
[`CONTRIBUTING.md`](CONTRIBUTING.md) covers the settled scope decisions and the
invariants worth knowing before changing behaviour.

## Licence

**SqeezeAmp is [MPL-2.0](LICENSE)** — all of it, including the UI. The complaint
this was started over is a player interface nobody can fix, extend or theme, and
this is the licence that keeps this one from becoming that.

The audio engine it drives is GPLv3 and is downloaded rather than distributed
here; Qt is used under LGPLv3. [`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md)
has the details.

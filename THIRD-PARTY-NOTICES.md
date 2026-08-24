# Third-party notices

Every binary artifact must ship this file and everything under
`packaging/licenses/`. `CMakeLists.txt` fails to configure if one is missing,
because a package that quietly omits its notices satisfies none of the
obligations below.

## squeezelite — GPLv3, and not distributed here

KvitSqueeze plays audio by supervising upstream
[squeezelite](https://github.com/ralph-irving/squeezelite) as a child process
at `engine/squeezelite.exe`. The binary is **unmodified**: KvitSqueeze talks to
it only through documented command-line arguments and its log output
(prd.md §7.3.2).

**No KvitSqueeze artifact contains it.** Neither the installer nor the portable
zip carries squeezelite, and it is not in this repository. Instead:

- the **installer downloads it during setup**, from upstream, verifying it
  against a pinned SHA-256 before use;
- the **portable zip carries `fetch-engine.ps1`**, which does the same thing
  when the user runs it.

Both read the location out of `packaging/engine-manifest.txt`, which is fetched
over the network from this project's repository rather than compiled in —
upstream prunes old builds from SourceForge, so a baked-in URL would eventually
404 with no way to repair the copies already installed. Editing that one file
repairs every installer ever shipped.

### What that does and does not change

GPLv3's obligations attach to **conveying** the program. An installer that
fetches a binary from its own author conveys nothing: the recipient obtains
squeezelite from upstream, on upstream's terms, exactly as if they had clicked
the link themselves. So KvitSqueeze owes no licence text, no corresponding
source, and no written offer for squeezelite — the arrangement this project
previously used, attaching an upstream source snapshot to each release under
GPLv3 §6(d), is no longer needed and no longer done.

What has **not** changed is the reasoning in prd.md §11.2 about the process
boundary. KvitSqueeze's own MPL-2.0 depends on squeezelite being a separate
program communicating at arm's length, so the rules that keep it separate still
bind: talk to the child only through documented CLI arguments and its log
output, never patch it, and keep it independently runnable and independently
obtainable. Downloading it from upstream rather than bundling it makes that
last point stronger than it was.

### What is done anyway, because it is cheap and honest

- `packaging/licenses/LICENSE.squeezelite` — the full, verbatim GPLv3 text —
  ships with every artifact, so a user whose installer fetched squeezelite has
  its terms on disk.
- Upstream's own `LICENSE.txt` from the downloaded archive is kept beside the
  binary as `engine/LICENSE.squeezelite.txt`.
- `packaging/engine-version.txt` records the exact build fetched, with its
  checksum, so the source that matches it can be identified.

### The version is pinned, and why

`ExternalEngine::applyLogLine()` scrapes squeezelite's log output for engine
status, and that format is not a stable interface. The manifest may only ever
be pointed at a build that has been run and checked — `tests/test_externalengine.cpp`
holds lines captured from exactly the pinned build, and is what says so when
they stop matching.

### What is inside that binary

The pinned build reports `WIN PORTAUDIO WINEVENT RESAMPLE FFMPEG OPUS OGGMETA
DSD SSL LINKALL`, so PortAudio (MIT), FLAC, libmad, faad2, libvorbis, libopus,
soxr and parts of FFmpeg are statically linked into it by its authors. Those
are components of *that* program, not of KvitSqueeze — none of them is compiled,
linked or vendored here, and none of them ships in a KvitSqueeze artifact.

This is also why prd.md §14 assumption 4 (the libmad / faad2 licence question)
is moot under Backend B: nothing in this repository compiles a codec.

## Qt — LGPLv3

Qt is used under LGPLv3, which requires dynamic linking and the ability for a
user to relink against their own Qt build. **No static Qt in a shipped
installer** — `win-deploy.bat` stages the Qt DLLs beside the executable, which
is what makes relinking possible. The LGPL texts come from the Qt kit at deploy
time.

## Segoe MDL2 Assets

The transport glyphs are characters from a font Windows ships with the shell.
The font is not redistributed: it is referenced by name and resolved on the
machine that runs the app.

## Not used

Recorded because they were evaluated and rejected, so the question does not
get reopened by accident (prd.md §11.3): no vendored squeezelite source, no
FFmpeg linked into this application, no standalone codec libraries, and no
QKeychain — the Windows Credential Manager is reached directly through three
Win32 calls (`src/session/credentialstore.cpp`). v1 decodes nothing itself.

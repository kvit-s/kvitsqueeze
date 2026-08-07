# Third-party notices

Every binary artifact must ship this file and everything under
`packaging/licenses/`. `CMakeLists.txt` fails to configure if one is missing,
because a package that quietly omits its notices satisfies none of the
obligations below.

## squeezelite — GPLv3

SqeezeAmp ships upstream [squeezelite](https://github.com/ralph-irving/squeezelite)
as `engine/squeezelite.exe` and runs it as a child process. The binary is
**unmodified**: SqeezeAmp talks to it only through documented command-line
arguments and its log output (prd.md §7.3.2).

Obligations that travel with the binary, whatever licence SqeezeAmp itself
carries:

- `packaging/licenses/LICENSE.squeezelite` — the full, verbatim GPLv3 text.
- `packaging/licenses/WRITTEN-OFFER.txt` — the offer to supply corresponding
  source, valid for the period GPLv3 §6 requires. **Two blanks in it must be
  filled in before a release**; a build that never leaves the machine that
  produced it incurs no obligation at all (prd.md §11.1).
- The exact upstream version shipped, recorded in
  `packaging/engine-version.txt` with its checksum, so the source that matches
  the binary can be identified.

### What is inside that binary

The shipped build reports `WIN PORTAUDIO WINEVENT RESAMPLE FFMPEG OPUS DSD SSL
LINKALL`, so PortAudio (MIT), FLAC, libmad, faad2, libvorbis, libopus, soxr and
parts of FFmpeg are statically linked into it by its authors. Those are
components of *that* program, not of SqeezeAmp — none of them is compiled,
linked or vendored here. Their corresponding source is part of squeezelite's
corresponding source, which the written offer covers.

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

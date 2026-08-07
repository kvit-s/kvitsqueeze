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

- `packaging/licenses/LICENSE.squeezelite` — the full GPLv3 text.
- `packaging/licenses/WRITTEN-OFFER.txt` — the offer to supply corresponding
  source, valid for the period GPLv3 §6 requires.
- The exact upstream version shipped, recorded in
  `packaging/engine-version.txt` with its checksum, so the source that matches
  the binary can be identified.

## Qt — LGPLv3

Qt is used under LGPLv3, which requires dynamic linking and the ability for a
user to relink against their own Qt build. **No static Qt in a shipped
installer.** The LGPL texts come from the Qt kit at deploy time.

## Not used

Recorded because they were evaluated and rejected, so the question does not
get reopened by accident (prd.md §11.3): no vendored squeezelite source, no
FFmpeg, no standalone codec libraries. v1 decodes nothing itself.

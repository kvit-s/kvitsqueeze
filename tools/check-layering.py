#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Check the module boundary the tree is built on.

src/ is five modules (see CMakeLists.txt, "The module boundary"). CMake
enforces the include direction already — each target publishes only its own
directory and inherits what it links, so an upward include fails to compile.
This script exists for the things that cannot express:

  * A wrong `target_link_libraries` line would silently make an upward include
    legal. The map below is the intended graph, checked against the sources
    rather than against CMake, so the two have to agree.

  * Three ownership rules are about what a module *does*, not what it
    includes:
      - Only sqz-session may reach Qt's networking, plus the two files listed
        in NETWORK_EXEMPT — see the note there.
      - Only sqz-engine may start a child process.
      - Only sqz-session may supply a player id to a request (prd.md FR-6.1).

Run it directly, or as the LayeringGuard ctest entry:

    python3 tools/check-layering.py

Exits non-zero and prints every violation with its file and line.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")

# ── The intended graph.
#
# A module may include headers from itself and from anything listed here.
#
# engine depends on protocol for one type only: PlayerIdentity. The engine
# still knows nothing about LMS — see the note in CMakeLists.txt — but it does
# have to know *which player it is*, because it passes that to squeezelite's
# -m. The session needs the same value for FR-6.1, and the player-id rule below
# forbids any module that can see both of them from carrying it. So both read
# it from the same place rather than one handing it to the other.
DEPENDS = {
    "protocol": set(),
    "engine": {"protocol"},
    "session": {"protocol"},
    "app": {"session", "engine", "protocol"},
    "qml": {"app", "session", "engine", "protocol"},
}

# src/main.cpp is the executable's entry point, not a module; it composes.
UNMODULED = {"main.cpp"}

# ── Qt networking: sqz-session only.
#
# Every request the app makes goes through LmsSession, which is also the only
# place a player id is injected. A module that could open its own connection
# would be a way around both rules.
NETWORK_TYPES = re.compile(
    r"\bQ(?:Network\w*|Ssl\w*|Http\w*"
    r"|HostAddress|HostInfo|Authenticator"
    r"|TcpSocket|TcpServer|UdpSocket)\b"
)
NETWORK_OWNER = "session"

# ── The one exception, and why it is a whitelist of two files rather than a
# module.
#
# KvitSqueeze does not distribute squeezelite: it is GPLv3, and neither shipped
# artifact carries it (THIRD-PARTY-NOTICES.md). Until FR-2.9 the only ways to
# get one were the installer's download step and a PowerShell script, which
# meant a portable user who did not read the README had a complete-looking app
# that would not make a sound. EngineInstaller is the app doing it itself.
#
# What that costs is one outbound HTTPS request from sqz-engine. It is not an
# LMS request, it carries no player id, and it cannot reach LmsSession — so the
# reason for the rule ("every request goes through the one place that injects
# the id") is intact, and prd.md N7 is untouched because N7 is about *listening*
# endpoints. Named by file so that a second networked thing in sqz-engine is a
# decision someone has to make here, not something that slips in.
NETWORK_EXEMPT = {
    "engine/engineinstaller.h",
    "engine/engineinstaller.cpp",
}

# ── Child processes: sqz-engine only.
#
# prd.md N7 — the app drives one child process, the audio engine, and nothing
# else. A QProcess anywhere above sqz-engine is a second one.
PROCESS_TYPES = re.compile(r"\bQProcess\b")
PROCESS_OWNER = "engine"

# ── Player identity: not above sqz-session (prd.md FR-6.1/FR-6.2).
#
# KvitSqueeze addresses exactly one player, itself. The id is injected in
# LmsSession::post() and nowhere else, so a call site cannot name a foreign
# player even by mistake.
#
# The rule is about *supplying* a value, not about the word appearing:
#
#   protocol  takes one as a parameter — it builds requests and cannot know
#             whose they are
#   session   owns the value and injects it
#   engine    passes it to squeezelite's -m
#
# What is banned is sqz-app and sqz-qml. A model or a QML binding that starts
# carrying a player id is the first step back toward a player switcher,
# whether or not it ever sends one.
PLAYER_ID_TYPES = re.compile(r"\b(?:playerId|player_id|setPlayerId)\b")
PLAYER_ID_OWNERS = {"protocol", "session", "engine"}

INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')


def module_of(path):
    relative = os.path.relpath(path, SRC)
    parts = relative.split(os.sep)
    if len(parts) == 1:
        return None  # main.cpp
    return parts[0]


def header_owners():
    """Map every header basename to the module that owns it."""
    owners = {}
    for directory, _subdirs, files in os.walk(SRC):
        for name in files:
            if name.endswith((".h", ".hpp")):
                module = module_of(os.path.join(directory, name))
                if module:
                    owners[name] = module
    return owners


def main():
    owners = header_owners()
    violations = []

    for directory, _subdirs, files in os.walk(SRC):
        for name in sorted(files):
            if not name.endswith((".h", ".hpp", ".cpp")):
                continue

            path = os.path.join(directory, name)
            module = module_of(path)
            if module is None and name not in UNMODULED:
                violations.append(f"{path}: sits directly in src/ but is not {UNMODULED}")
                continue
            if module is None:
                continue

            relative = os.path.relpath(path, ROOT).replace(os.sep, "/")
            within_src = os.path.relpath(path, SRC).replace(os.sep, "/")
            network_exempt = within_src in NETWORK_EXEMPT
            with open(path, encoding="utf-8") as handle:
                for number, line in enumerate(handle, 1):
                    match = INCLUDE.match(line)
                    if match:
                        included = os.path.basename(match.group(1))
                        owner = owners.get(included)
                        if owner and owner != module and owner not in DEPENDS[module]:
                            violations.append(
                                f"{relative}:{number}: {module} includes {included}, "
                                f"which belongs to {owner} — the graph does not allow it"
                            )

                    if (module != NETWORK_OWNER and not network_exempt
                            and NETWORK_TYPES.search(line)):
                        violations.append(
                            f"{relative}:{number}: Qt networking outside sqz-{NETWORK_OWNER}"
                        )

                    if module != PROCESS_OWNER and PROCESS_TYPES.search(line):
                        violations.append(
                            f"{relative}:{number}: QProcess outside sqz-{PROCESS_OWNER}"
                        )

                    if module not in PLAYER_ID_OWNERS and PLAYER_ID_TYPES.search(line):
                        violations.append(
                            f"{relative}:{number}: a player id outside "
                            f"{sorted(PLAYER_ID_OWNERS)} — KvitSqueeze addresses one "
                            f"player and LmsSession injects its id (prd.md FR-6.1)"
                        )

    if violations:
        print("Module boundary violations:\n")
        for violation in violations:
            print("  " + violation)
        print(f"\n{len(violations)} violation(s).")
        return 1

    print("Module boundary: clean.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

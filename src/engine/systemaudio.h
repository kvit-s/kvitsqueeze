// SPDX-License-Identifier: MPL-2.0

#pragma once

// The playback device Windows itself would use, named the way the audio engine
// names it.
//
// This exists because "no device specified" does not mean "the system default"
// to the engine. Started without one, squeezelite opens PortAudio's own default
// — the first MME device — which on a machine with an HDMI display is usually
// the monitor rather than whatever the user is listening on. The failure is
// silent in the worst way: the server streams, the transport runs, the position
// advances, and no sound comes out.
//
// So "System default" in the settings screen is resolved here and passed
// explicitly, and it resolves to the WASAPI endpoint, which is what prd.md
// FR-2.4 says the product plays through.
//
// Returns an empty string when there is no default endpoint, or on a platform
// with no such concept. That is prd.md FR-2.5's rule applied to a device name:
// an unknown stays unknown, and the caller falls back rather than guessing.

#include <QString>

QString systemDefaultOutputDevice();

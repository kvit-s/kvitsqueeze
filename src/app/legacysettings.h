// SPDX-License-Identifier: MPL-2.0

#pragma once

// The project was called SqeezeAmp until it collided with philippe44/SqueezeAMP,
// an ESP32 audio board in the same ecosystem. Renaming it moved the settings
// tree from HKCU\Software\SqeezeAmp\SqeezeAmp to the KvitSqueeze equivalent,
// and that tree holds more than preferences: it holds this installation's
// generated player identity, which is what makes the server treat it as the
// same player across restarts and keep its queue (prd.md FR-1.4).
//
// Without a copy, upgrading looks like buying a new player: the old one lingers
// in the server's list holding the queue, and a fresh one appears empty.
//
// Delete this when no installation could still be on a pre-rename build.
namespace LegacySettings {

// Copies the pre-rename settings tree into the current one, if and only if the
// current one is empty. Returns true if anything was copied.
//
// Must be called after QCoreApplication's organisation and application names
// are set, and before anything constructs a Settings.
//
// The old tree is left in place: a user who goes back to the previous build
// should find their settings where that build left them.
bool migrateIfNeeded();

} // namespace LegacySettings

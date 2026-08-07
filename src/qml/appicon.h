#pragma once

// The application icon, drawn rather than shipped.
//
// A painted icon keeps the tree free of a binary asset and, more usefully,
// scales to whatever size the tray, the taskbar and the Alt-Tab switcher ask
// for on a 200% display without a set of pre-rendered sizes to maintain
// (prd.md FR-8.3). It is deliberately plain: prd.md N8 rules out skinning, so
// there is one look and nothing to make pluggable.
//
// It carries no Squeezebox or Logitech mark — prd.md §11.4 keeps both out of
// the name and the icon.

#include <QIcon>

namespace AppIcon {

// The app and tray icon. `muted` draws the paused variant, which is what the
// tray shows when the player is powered off (prd.md FR-6.3 — a powered-off
// player must look powered off, not hung).
QIcon application(bool muted = false);

} // namespace AppIcon

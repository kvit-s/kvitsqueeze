// SPDX-License-Identifier: MPL-2.0

#pragma once

// prd.md FR-1.3: the control API's password lives in the Windows Credential
// Manager, never in plaintext config.
//
// The PRD names QKeychain. This uses the Win32 credential API directly
// instead, because QKeychain's Windows backend *is* these three calls plus a
// build dependency, and prd.md NFR-8 asks for a build with no vcpkg manifest
// to drift. The seam is the same either way: a store with three operations, so
// swapping the implementation is one file.
//
// It lives in sqz-session because the credential is the session's — the same
// module that injects it into a request is the one that reads it back, and
// nothing above ever holds a password.
//
// On a platform without a credential store the calls fail rather than falling
// back to a file. A password silently written to disk is worse than a password
// the user has to type again (prd.md N1 keeps that hypothetical).

#include <QString>

namespace CredentialStore
{

// One entry per server, so switching servers does not overwrite the other's
// password. The target name is derived from host and port.
bool save(const QString &host, quint16 port, const QString &user, const QString &password);

// Returns false when there is nothing stored, which is the normal case: the
// server this was developed against needs no auth at all (prd.md §13 Q6).
bool load(const QString &host, quint16 port, QString *user, QString *password);

bool remove(const QString &host, quint16 port);

// False where no credential store exists, so the settings UI can say "not
// available on this platform" instead of failing every save.
bool isAvailable();

} // namespace CredentialStore

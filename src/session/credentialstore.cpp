// SPDX-License-Identifier: MPL-2.0

#include "credentialstore.h"

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#  include <wincred.h>
#endif

namespace {

QString targetName(const QString &host, quint16 port)
{
    return QStringLiteral("KvitSqueeze/%1:%2").arg(host).arg(port);
}

} // namespace

namespace CredentialStore {

#ifdef Q_OS_WIN

bool isAvailable() { return true; }

bool save(const QString &host, quint16 port, const QString &user, const QString &password)
{
    if (host.isEmpty())
        return false;

    if (user.isEmpty())
        return remove(host, port);

    const QString target = targetName(host, port);
    const QByteArray blob(reinterpret_cast<const char *>(password.utf16()),
                          static_cast<int>(password.size() * sizeof(char16_t)));

    CREDENTIALW credential = {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(target.utf16()));
    credential.UserName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(user.utf16()));
    credential.CredentialBlobSize = static_cast<DWORD>(blob.size());
    credential.CredentialBlob =
        reinterpret_cast<LPBYTE>(const_cast<char *>(blob.constData()));
    // LOCAL_MACHINE rather than ENTERPRISE: this password is for a server on
    // the user's own network and has no business roaming to a domain profile.
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

    return CredWriteW(&credential, 0) != FALSE;
}

bool load(const QString &host, quint16 port, QString *user, QString *password)
{
    const QString target = targetName(host, port);

    PCREDENTIALW credential = nullptr;
    if (!CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()),
                   CRED_TYPE_GENERIC, 0, &credential))
        return false;

    if (user && credential->UserName)
        *user = QString::fromWCharArray(credential->UserName);
    if (password && credential->CredentialBlob) {
        // The blob is UTF-16 without a terminator, so the length comes from
        // the size field. Treating it as a C string reads past the end.
        *password = QString::fromUtf16(
            reinterpret_cast<const char16_t *>(credential->CredentialBlob),
            static_cast<qsizetype>(credential->CredentialBlobSize / sizeof(char16_t)));
    }

    CredFree(credential);
    return true;
}

bool remove(const QString &host, quint16 port)
{
    const QString target = targetName(host, port);
    return CredDeleteW(reinterpret_cast<LPCWSTR>(target.utf16()),
                       CRED_TYPE_GENERIC, 0) != FALSE;
}

#else

// prd.md N1 keeps the tree portable without shipping a second platform. A
// build that gets here has no credential store, and inventing one that writes
// to a file would be worse than having none.
bool isAvailable() { return false; }
bool save(const QString &, quint16, const QString &, const QString &) { return false; }
bool load(const QString &, quint16, QString *, QString *) { return false; }
bool remove(const QString &, quint16) { return false; }

#endif

} // namespace CredentialStore

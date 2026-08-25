// SPDX-License-Identifier: MPL-2.0

#pragma once

// Fetching the audio engine, from inside the app (prd.md FR-2.11).
//
// KvitSqueeze does not distribute squeezelite: it is GPLv3, and neither the
// installer nor the portable zip carries it (THIRD-PARTY-NOTICES.md). Until
// now the only ways to get one were the installer's own download step and
// packaging\windows\fetch-engine.ps1 — which meant a portable user who did not
// read the README ended up with an app that browsed a whole library and
// refused to make a sound. This class is the third way, and the only one the
// user is ever shown: a button.
//
// It does exactly what fetch-engine.ps1 does, for the same reasons, and the
// comments in packaging/engine-manifest.txt are the reasoning behind all of
// it:
//
//   * The download location is read over the network rather than compiled in,
//     because upstream prunes old builds from SourceForge and a baked-in URL
//     rots. A copy compiled in at build time is the fallback for an
//     unreachable GitHub, exactly as in the Inno Setup script.
//   * The User-Agent must not look like a browser. SourceForge answers those
//     with an HTML interstitial, and a 135 KB HTML page failing a checksum is
//     a far more confusing failure than a 404.
//   * Both SHA-256s are checked — the archive's and the executable's inside
//     it. Nothing unverified is ever written to disk under the name the
//     engine will later be launched from.
//
// ── Two boundary notes
//
// **Qt networking.** tools/check-layering.py normally forbids it outside
// sqz-session, because every LMS request goes through LmsSession, which is
// where the player id is injected. This file is the one documented exception
// and is named in that script: it makes a single outbound HTTPS request that
// is not an LMS request, carries no player id, and cannot reach the session.
//
// **prd.md N7.** N7 is about KvitSqueeze exposing no *control surface*: no
// listening endpoint but the single-instance named pipe. A user-initiated,
// one-shot outbound fetch adds no endpoint and answers to nobody, so N7 is
// untouched. It does widen "all network activity is outbound to the LMS
// server" by one request, which is why that sentence in prd.md now says so.
//
// **Licensing.** Downloading is not conveying. This changes nothing about
// §11.2: fetching a GPLv3 program from its upstream on the user's behalf puts
// no GPL code in any KvitSqueeze artifact, which is precisely what the
// installer has always done.

#include <QObject>
#include <QString>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

class EngineInstaller : public QObject
{
    Q_OBJECT

public:
    // packaging/engine-manifest.txt, parsed. "One key = value per line, # is a
    // comment" — deliberately trivial, because the same bytes are parsed here,
    // in Pascal by the installer, and in PowerShell by fetch-engine.ps1.
    struct Manifest
    {
        QString version;
        QString url;
        QString sha256;        // of the archive
        QString member;        // the executable's name inside the archive
        QString memberSha256;  // of that executable; optional in the format

        bool isValid() const
        {
            return !url.isEmpty() && !sha256.isEmpty() && !member.isEmpty();
        }
    };

    explicit EngineInstaller(QObject *parent = nullptr);
    ~EngineInstaller() override;

    // Where a successful install writes squeezelite.exe. Set by the engine,
    // which is the only thing that knows where it later looks for one.
    void setDestination(const QString &executablePath);
    QString destination() const { return m_destination; }

    bool isBusy() const { return m_busy; }

    // 0-100 while a body is being read, or -1 when the server sent no length.
    // prd.md FR-2.5's rule, applied to a progress bar: unknown is not zero.
    int progress() const { return m_progress; }

    // One line of plain English for the panel. Never an exception message.
    QString statusText() const { return m_status; }

    // Empty unless the last attempt failed. Kept after a failure so the panel
    // can go on showing why while offering the manual path instead.
    QString lastError() const { return m_error; }

    // The archive URL last read from a manifest, so a user whose firewall
    // blocked the download can still be told what to go and get. Empty until
    // a manifest has been read.
    QString sourceUrl() const { return m_manifest.url; }
    QString version() const { return m_manifest.version; }

    // Fetch, verify and install. Answers on finished().
    void install();
    void cancel();

    // Adopt a squeezelite.exe the user already has — the path that survives a
    // firewall, which is not a hypothetical: this app's first user found it by
    // having one. Copies rather than remembers a path, so there is exactly one
    // place the engine is ever launched from.
    bool installFrom(const QString &sourceExecutable);

    // Pure, so the format has cases in a test rather than hope. Unknown keys
    // are ignored: the installer and the PowerShell script read the same file
    // and one of them may learn a key first.
    static Manifest parseManifest(const QString &text);

    // The manifest compiled in at build time, for an unreachable GitHub.
    static QString bundledManifestText();

Q_SIGNALS:
    // Anything a panel binds to: busy, progress, status, error.
    void changed();
    void finished(bool ok);

private:
    void fail(const QString &message);
    void setStatus(const QString &status);
    void setBusy(bool busy);
    void setProgress(int progress);
    void requestManifest();
    void haveManifest(const QString &text, bool fromNetwork);
    void requestArchive();
    void haveArchive();
    bool unpack(const QByteArray &archive);
    void abandonReply();

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QString m_destination;
    Manifest m_manifest;
    QString m_status;
    QString m_error;
    int m_progress = -1;
    bool m_busy = false;
};

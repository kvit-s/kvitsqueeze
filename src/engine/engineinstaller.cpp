// SPDX-License-Identifier: MPL-2.0

#include "engineinstaller.h"

#include "enginemanifest.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStringList>
#include <QUrl>

#include <QtCore/private/qzipreader_p.h>

namespace {

// The project's own copy of the manifest, which is what makes a build pruned
// from SourceForge repairable without a release. See packaging/engine-manifest.txt.
constexpr auto kManifestUrl =
    "https://raw.githubusercontent.com/kvit-s/kvitsqueeze/main/packaging/engine-manifest.txt";

// Not a browser. SourceForge answers browser-shaped User-Agents with an HTML
// "your download will start shortly" page instead of the file — the same
// string fetch-engine.ps1 sends, for the same reason.
constexpr auto kUserAgent = "KvitSqueeze-fetch-engine";

QString hexSha256(const QByteArray &data)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

// A PE image starts "MZ". Checked before anything is written under the name
// the engine will later be launched from, and checked on the manual path too,
// where there is no checksum to lean on.
bool looksExecutable(const QByteArray &data)
{
    return data.size() > 2 && data[0] == 'M' && data[1] == 'Z';
}

} // namespace

EngineInstaller::EngineInstaller(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

EngineInstaller::~EngineInstaller() = default;

void EngineInstaller::setDestination(const QString &executablePath)
{
    m_destination = executablePath;
}

QString EngineInstaller::bundledManifestText()
{
    return QString::fromUtf8(sqzBundledEngineManifest());
}

EngineInstaller::Manifest EngineInstaller::parseManifest(const QString &text)
{
    Manifest manifest;
    // Split on '\n' and trim, rather than a CRLF-aware regex: the trim has to
    // happen anyway, and this file is written by hand on Windows.
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString stripped = raw.section(QLatin1Char('#'), 0, 0).trimmed();
        if (stripped.isEmpty())
            continue;
        const qsizetype split = stripped.indexOf(QLatin1Char('='));
        if (split < 0)
            continue;

        const QString key = stripped.left(split).trimmed().toLower();
        const QString value = stripped.mid(split + 1).trimmed();

        if (key == QLatin1String("version"))
            manifest.version = value;
        else if (key == QLatin1String("url"))
            manifest.url = value;
        else if (key == QLatin1String("sha256"))
            manifest.sha256 = value.toLower();
        else if (key == QLatin1String("member"))
            manifest.member = value;
        else if (key == QLatin1String("member_sha256"))
            manifest.memberSha256 = value.toLower();
        // Anything else is ignored on purpose: three programs read this file
        // and one of them may learn a key before the others do.
    }
    return manifest;
}

void EngineInstaller::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    Q_EMIT changed();
}

void EngineInstaller::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    Q_EMIT changed();
}

void EngineInstaller::setProgress(int progress)
{
    if (m_progress == progress)
        return;
    m_progress = progress;
    Q_EMIT changed();
}

void EngineInstaller::fail(const QString &message)
{
    abandonReply();
    m_error = message;
    m_status = tr("The audio engine could not be installed.");
    m_progress = -1;
    m_busy = false;
    Q_EMIT changed();
    Q_EMIT finished(false);
}

void EngineInstaller::abandonReply()
{
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
}

void EngineInstaller::cancel()
{
    if (!m_busy)
        return;
    abandonReply();
    m_error.clear();
    m_status = tr("Cancelled.");
    m_progress = -1;
    m_busy = false;
    Q_EMIT changed();
    Q_EMIT finished(false);
}

void EngineInstaller::install()
{
    if (m_busy)
        return;
    if (m_destination.isEmpty()) {
        fail(tr("There is nowhere to put the audio engine."));
        return;
    }

    m_error.clear();
    m_progress = -1;
    setBusy(true);
    requestManifest();
}

void EngineInstaller::requestManifest()
{
    setStatus(tr("Looking up the current build…"));

    QNetworkRequest request{QUrl(QString::fromLatin1(kManifestUrl))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setTransferTimeout(30000);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_reply;
        if (!reply)
            return;
        m_reply = nullptr;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            // Not a failure yet. The whole point of the compiled-in copy is
            // that an unreachable GitHub costs the repairability of the URL,
            // not the download.
            haveManifest(bundledManifestText(), false);
            return;
        }
        haveManifest(QString::fromUtf8(reply->readAll()), true);
    });
}

void EngineInstaller::haveManifest(const QString &text, bool fromNetwork)
{
    m_manifest = parseManifest(text);
    if (!m_manifest.isValid()) {
        if (fromNetwork) {
            // A published manifest that does not parse is worse than no
            // published manifest: fall back rather than stop.
            haveManifest(bundledManifestText(), false);
            return;
        }
        fail(tr("No usable download location is available."));
        return;
    }
    Q_EMIT changed();  // sourceUrl() and version() are answerable now
    requestArchive();
}

void EngineInstaller::requestArchive()
{
    setStatus(m_manifest.version.isEmpty()
                  ? tr("Downloading the audio engine…")
                  : tr("Downloading squeezelite %1…").arg(m_manifest.version));

    QNetworkRequest request{QUrl(m_manifest.url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    // An inactivity timeout, not a deadline: a slow link must not be told the
    // download failed halfway through a file that is still arriving.
    request.setTransferTimeout(60000);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                // prd.md FR-2.5's rule applied to a progress bar: a server
                // that sent no length leaves this unknown rather than at 0.
                setProgress(total > 0 ? int((received * 100) / total) : -1);
            });
    connect(m_reply, &QNetworkReply::finished, this, &EngineInstaller::haveArchive);
}

void EngineInstaller::haveArchive()
{
    QNetworkReply *reply = m_reply;
    if (!reply)
        return;
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        fail(tr("The download failed: %1").arg(reply->errorString()));
        return;
    }

    const QByteArray archive = reply->readAll();

    // Shape before checksum. A mirror answering with an HTML error page fails
    // a checksum, which reads as "the file was tampered with" when the truth
    // is "that is not a file" — and behind a corporate firewall an
    // interception page is the likeliest thing to arrive here.
    if (archive.size() < 2 || archive[0] != 'P' || archive[1] != 'K') {
        fail(tr("What arrived is not the audio engine — %1 bytes that are not "
                "an archive. A proxy or firewall may have answered instead of "
                "the download site.")
                 .arg(archive.size()));
        return;
    }

    setProgress(-1);
    setStatus(tr("Checking the download…"));

    if (hexSha256(archive) != m_manifest.sha256) {
        fail(tr("The download does not match its published checksum, so it has "
                "not been installed."));
        return;
    }

    if (!unpack(archive))
        return;

    m_error.clear();
    m_progress = -1;
    m_busy = false;
    m_status = m_manifest.version.isEmpty()
                   ? tr("The audio engine is installed. Playback is ready.")
                   : tr("squeezelite %1 is installed. Playback is ready.")
                         .arg(m_manifest.version);
    Q_EMIT changed();
    Q_EMIT finished(true);
}

bool EngineInstaller::unpack(const QByteArray &archive)
{
    setStatus(tr("Installing…"));

    // The buffer has to be open before QZipReader is handed it: the reader
    // takes the device as it finds it and does not open one for itself, so an
    // unopened buffer reads as a corrupt archive — a 3 MB download that passed
    // its checksum a line earlier, reported as unopenable.
    QBuffer buffer;
    buffer.setData(archive);
    if (!buffer.open(QIODevice::ReadOnly)) {
        fail(tr("The downloaded archive could not be read."));
        return false;
    }

    QZipReader zip(&buffer);
    if (!zip.isReadable()) {
        fail(tr("The downloaded archive could not be opened."));
        return false;
    }

    // The manifest names the member because upstream names the executable for
    // the build variant rather than "squeezelite.exe". Matched on the whole
    // path first, then on the last component, so an archive that grows a
    // top-level folder does not need a new manifest key.
    QString memberPath;
    const QList<QZipReader::FileInfo> entries = zip.fileInfoList();
    for (const QZipReader::FileInfo &entry : entries) {
        if (!entry.isFile)
            continue;
        if (entry.filePath == m_manifest.member
            || entry.filePath.endsWith(QLatin1Char('/') + m_manifest.member)) {
            memberPath = entry.filePath;
            break;
        }
    }
    if (memberPath.isEmpty()) {
        fail(tr("The archive does not contain %1.").arg(m_manifest.member));
        return false;
    }

    const QByteArray engine = zip.fileData(memberPath);
    if (!m_manifest.memberSha256.isEmpty() && hexSha256(engine) != m_manifest.memberSha256) {
        fail(tr("The program inside the archive does not match its published "
                "checksum, so it has not been installed."));
        return false;
    }
    if (!looksExecutable(engine)) {
        fail(tr("The program inside the archive is not a Windows executable."));
        return false;
    }

    const QFileInfo target(m_destination);
    if (!QDir().mkpath(target.absolutePath())) {
        fail(tr("The folder %1 could not be created.").arg(target.absolutePath()));
        return false;
    }

    // QSaveFile rather than QFile: a half-written squeezelite.exe at the path
    // the engine launches from would be worse than none at all, because
    // isAvailable() only asks whether the file is there.
    QSaveFile out(m_destination);
    if (!out.open(QIODevice::WriteOnly) || out.write(engine) != engine.size()
        || !out.commit()) {
        fail(tr("%1 could not be written. %2").arg(m_destination, out.errorString()));
        return false;
    }

    // Upstream ships its licence text in the archive. It costs nothing to keep
    // it beside the binary, and a user who now has a GPLv3 program on disk
    // should have its terms too — the same thing fetch-engine.ps1 does.
    const QByteArray licence = zip.fileData(QStringLiteral("LICENSE.txt"));
    if (!licence.isEmpty()) {
        QSaveFile notice(target.absolutePath() + QStringLiteral("/LICENSE.squeezelite.txt"));
        if (notice.open(QIODevice::WriteOnly)) {
            notice.write(licence);
            notice.commit();
        }
    }

    return true;
}

bool EngineInstaller::installFrom(const QString &sourceExecutable)
{
    if (m_busy)
        return false;
    if (m_destination.isEmpty()) {
        fail(tr("There is nowhere to put the audio engine."));
        return false;
    }

    QFile source(sourceExecutable);
    if (!source.open(QIODevice::ReadOnly)) {
        fail(tr("%1 could not be read.").arg(sourceExecutable));
        return false;
    }
    const QByteArray engine = source.readAll();
    source.close();

    if (!looksExecutable(engine)) {
        fail(tr("%1 is not a Windows program.").arg(QFileInfo(sourceExecutable).fileName()));
        return false;
    }

    const QFileInfo target(m_destination);
    if (QFileInfo(sourceExecutable).canonicalFilePath() == target.canonicalFilePath()) {
        // Already the file the engine launches. Nothing to copy, and copying
        // a file over itself truncates it.
        m_error.clear();
        m_status = tr("That is already the engine this app uses.");
        Q_EMIT changed();
        Q_EMIT finished(true);
        return true;
    }

    if (!QDir().mkpath(target.absolutePath())) {
        fail(tr("The folder %1 could not be created.").arg(target.absolutePath()));
        return false;
    }

    QSaveFile out(m_destination);
    if (!out.open(QIODevice::WriteOnly) || out.write(engine) != engine.size()
        || !out.commit()) {
        fail(tr("%1 could not be written. %2").arg(m_destination, out.errorString()));
        return false;
    }

    m_error.clear();
    m_progress = -1;
    m_status = tr("The audio engine is installed. Playback is ready.");
    Q_EMIT changed();
    Q_EMIT finished(true);
    return true;
}

#include "lmssession.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

LmsSession::LmsSession(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void LmsSession::setServer(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
}

void LmsSession::setPlayerId(const QString &playerId)
{
    m_playerId = playerId;
}

QUrl LmsSession::serverUrl() const
{
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(m_host);
    url.setPort(m_port);
    return url;
}

QUrl LmsSession::artworkUrl(const QString &coverId, int size) const
{
    QUrl url = serverUrl();
    // ⚠️ prd.md §14 item 1: confirm this resize-suffix grammar against the
    // installed server version before relying on it.
    url.setPath(QStringLiteral("/music/%1/cover_%2x%2_o.jpg").arg(coverId).arg(size));
    return url;
}

void LmsSession::send(const QStringList &command)
{
    post(m_playerId, command);
}

void LmsSession::sendServerScoped(const QStringList &command)
{
    post(QString(), command);
}

void LmsSession::post(const QString &playerId, const QStringList &command)
{
    if (m_host.isEmpty()) {
        Q_EMIT connectionError(tr("No server configured"));
        return;
    }

    QUrl url = serverUrl();
    url.setPath(QStringLiteral("/jsonrpc.js"));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    const QJsonDocument body(LmsRequest::jsonRpcBody(playerId, command));
    QNetworkReply *reply = m_network->post(request, body.toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, command] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT connectionError(reply->errorString());
            return;
        }

        const QJsonObject root =
            QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject result = root.value(QStringLiteral("result")).toObject();

        // Only a `status` reply carries a player snapshot. Everything else is
        // an acknowledgement, and the authoritative state arrives on the
        // event stream rather than here (prd.md §7.4).
        if (!command.isEmpty() && command.first() == QLatin1String("status"))
            Q_EMIT statusReceived(PlayerStatus::fromStatusResult(result));
    });
}

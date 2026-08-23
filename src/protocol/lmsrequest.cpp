// SPDX-License-Identifier: MPL-2.0

#include "lmsrequest.h"

#include <QJsonArray>
#include <QUrl>

namespace {

// The CLI's encoding is QUrl's, restricted to the characters that would
// otherwise break tokenisation or the line framing. Percent-encoding
// everything would also be correct but makes a captured session unreadable in
// a log, and the diagnostics panel (prd.md FR-9.2) shows these lines verbatim.
QByteArray encodeToken(const QString &token)
{
    return QUrl::toPercentEncoding(token, QByteArray(), " %\r\n");
}

} // namespace

namespace LmsRequest {

QJsonObject jsonRpcBody(const QString &playerId, const QStringList &command, int id)
{
    QJsonArray commandArray;
    for (const QString &token : command)
        commandArray.append(token);

    QJsonArray params;
    params.append(playerId);
    params.append(commandArray);

    QJsonObject body;
    body.insert(QStringLiteral("id"), id);
    body.insert(QStringLiteral("method"), QStringLiteral("slim.request"));
    body.insert(QStringLiteral("params"), params);
    return body;
}

QByteArray cliLine(const QString &playerId, const QStringList &command)
{
    QByteArray line;
    if (!playerId.isEmpty())
        line += encodeToken(playerId);

    for (const QString &token : command) {
        if (!line.isEmpty())
            line += ' ';
        line += encodeToken(token);
    }

    line += '\n';
    return line;
}

QStringList parseCliLine(const QByteArray &line)
{
    QByteArray trimmed = line;
    while (trimmed.endsWith('\n') || trimmed.endsWith('\r'))
        trimmed.chop(1);

    if (trimmed.isEmpty())
        return {};

    QStringList tokens;
    for (const QByteArray &token : trimmed.split(' ')) {
        // A double space would otherwise produce an empty token that no
        // command vocabulary contains.
        if (token.isEmpty())
            continue;
        tokens.append(QUrl::fromPercentEncoding(token));
    }
    return tokens;
}

} // namespace LmsRequest

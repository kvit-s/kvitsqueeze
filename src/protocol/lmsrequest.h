// SPDX-License-Identifier: MPL-2.0

#pragma once

// Building the two request forms LMS accepts, and decoding what comes back on
// the CLI socket. Pure functions of their input: nothing here opens a socket,
// reads a setting or knows a player exists. That is what lets the whole
// protocol vocabulary be tested before any of the app is written (prd.md M1).
//
// LMS separates control from streaming (prd.md §6.1). This file is the
// control side, in both of its transports:
//
//   JSON-RPC  POST http://<server>:9000/jsonrpc.js — request/response only.
//   CLI       TCP  <server>:9090 — the same command vocabulary as text lines,
//                  plus `subscribe`/`listen` for an async event stream, which
//                  is why the CLI socket exists at all (prd.md §13 Q1).
//
// Both take the same shape: a player id, then a command as a list of tokens.

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace LmsRequest {

// The JSON-RPC body for one command:
//   {"id":1,"method":"slim.request","params":["<playerId>",["status","-",1]]}
//
// An empty playerId is the server scope, which LMS spells as "". It is a
// legitimate value for server-wide commands such as `serverstatus`, so this
// does not reject it — the rule that SqeezeAmp only ever addresses its own
// player is enforced one layer up, in LmsSession (prd.md FR-6.1).
QJsonObject jsonRpcBody(const QString &playerId, const QStringList &command, int id = 1);

// One CLI line, ready to write to the socket, newline included.
//
// The CLI percent-encodes every token, which is not decoration: a track title
// containing a space would otherwise split into two tokens and the reply
// would parse as a different command's. Encoding is applied per token, after
// the player id, exactly as the server does it.
QByteArray cliLine(const QString &playerId, const QStringList &command);

// Split one CLI response line back into decoded tokens. The trailing newline
// is optional so this can be fed either a framed line or a whole read.
QStringList parseCliLine(const QByteArray &line);

} // namespace LmsRequest

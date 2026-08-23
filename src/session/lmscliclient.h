// SPDX-License-Identifier: MPL-2.0

#pragma once

// The push event stream: a plain TCP socket to the server's CLI on port 9090
// (prd.md §13 Q1, settled in favour of CLI over CometD).
//
// Why this and not CometD: the CLI is a line protocol on a socket that stays
// open, so framing is a split on '\n' and there is no Bayeux handshake,
// channel subscription, long-poll timeout or advice field to get right. It was
// checked against Lyrion Music Server 9.1.0 before the decision was made —
// transport, queue, volume and power changes driven from the server's own web
// UI all arrived (prd.md §14 assumption 2).
//
// What arrives is a *command*, not a state snapshot. This class does no
// interpretation beyond framing and decoding; deciding what an event means is
// CliEvent's job and acting on it is LmsSession's.
//
// Note what this is not: a listening socket. It dials out to the server and
// nothing dials in (prd.md N7 / NFR-10).

#include "clievent.h"

#include <QObject>
#include <QString>

class QTcpSocket;
class QTimer;

class LmsCliClient : public QObject
{
    Q_OBJECT

public:
    explicit LmsCliClient(QObject *parent = nullptr);

    void setServer(const QString &host, quint16 port = 9090);

    // prd.md FR-1.3 — the CLI has its own login, separate from the HTTP
    // basic-auth on the control port. Empty credentials skip the login line.
    void setCredentials(const QString &user, const QString &password);

    void start();
    void stop();

    bool isConnected() const;

Q_SIGNALS:
    void eventReceived(const CliEvent &event);
    void connected();
    void disconnected(const QString &reason);
    void trafficLogged(bool outgoing, const QString &text);

private:
    void onConnected();
    void onReadyRead();
    void write(const QByteArray &line);

    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
    QString m_host;
    quint16 m_port = 9090;
    QString m_user;
    QString m_password;
    bool m_wanted = false;
};

// SPDX-License-Identifier: MPL-2.0

#include "lmscliclient.h"

#include "lmsrequest.h"

#include <QTcpSocket>

namespace {

// A CLI line is short; anything past this is a server that is not speaking the
// protocol, and buffering it forever is how a stuck connection turns into an
// out-of-memory report.
constexpr int kMaxLineLength = 1 << 20;

} // namespace

LmsCliClient::LmsCliClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &LmsCliClient::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &LmsCliClient::onReadyRead);

    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        m_buffer.clear();
        Q_EMIT disconnected(m_socket->errorString());
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        m_buffer.clear();
        Q_EMIT disconnected(tr("The event stream closed"));
    });
}

void LmsCliClient::setServer(const QString &host, quint16 port)
{
    if (m_host == host && m_port == port)
        return;

    m_host = host;
    m_port = port;
    if (m_wanted) {
        stop();
        start();
    }
}

void LmsCliClient::setCredentials(const QString &user, const QString &password)
{
    m_user = user;
    m_password = password;
}

void LmsCliClient::start()
{
    m_wanted = true;
    if (m_host.isEmpty() || m_socket->state() != QAbstractSocket::UnconnectedState)
        return;
    m_socket->connectToHost(m_host, m_port);
}

void LmsCliClient::stop()
{
    m_wanted = false;
    m_buffer.clear();
    m_socket->abort();
}

bool LmsCliClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void LmsCliClient::write(const QByteArray &line)
{
    m_socket->write(line);
    Q_EMIT trafficLogged(true, QString::fromUtf8(line).trimmed());
}

void LmsCliClient::onConnected()
{
    if (!m_user.isEmpty())
        write(LmsRequest::cliLine(QString(), { QStringLiteral("login"), m_user, m_password }));

    // `listen 1` rather than `subscribe <names>`: the name list is a server
    // version's vocabulary, and a name this build has not heard of is silently
    // dropped rather than rejected, so a missing event would look like a bug
    // in this app. Everything arrives instead and CliEvent decides what
    // matters — the extra traffic is a handful of short lines.
    write(LmsRequest::cliLine(QString(), { QStringLiteral("listen"), QStringLiteral("1") }));

    Q_EMIT connected();
}

void LmsCliClient::onReadyRead()
{
    m_buffer += m_socket->readAll();

    int newline = 0;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);

        if (line.isEmpty())
            continue;

        Q_EMIT trafficLogged(false, QString::fromUtf8(line).trimmed());

        const CliEvent event = CliEvent::parse(line);
        if (!event.tokens.isEmpty())
            Q_EMIT eventReceived(event);
    }

    if (m_buffer.size() > kMaxLineLength) {
        m_buffer.clear();
        m_socket->abort();
        Q_EMIT disconnected(tr("The event stream sent an unterminated line"));
    }
}

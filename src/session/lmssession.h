#pragma once

// The live connection to Lyrion Music Server, and the one place a player id
// is ever supplied.
//
// prd.md FR-6.1: every control command carries this app's own player id, and
// no call site may pass a different one. That is why send() takes only a
// command — there is no parameter for a player, so a model or a QML binding
// has nothing to pass even by mistake. tests/test_lmssession.cpp asserts no
// request escapes with a foreign id.
//
// This is also the only module that links Qt6::Network (see CMakeLists.txt).

#include "lmsrequest.h"
#include "playerstatus.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class LmsSession : public QObject
{
    Q_OBJECT

public:
    explicit LmsSession(QObject *parent = nullptr);

    void setServer(const QString &host, quint16 port = 9000);
    void setPlayerId(const QString &playerId);

    QString playerId() const { return m_playerId; }
    QUrl serverUrl() const;

    // Send a control command to *our* player. The id is injected here.
    void send(const QStringList &command);

    // Server-scoped commands, which LMS addresses with an empty player id.
    // Separate from send() so that reaching the server scope is a deliberate
    // call rather than passing "" to the normal path.
    void sendServerScoped(const QStringList &command);

    // The artwork URL for a cover id (prd.md §6.2). Kept here because it is
    // the same host and credentials as the control API.
    QUrl artworkUrl(const QString &coverId, int size) const;

Q_SIGNALS:
    void statusReceived(const PlayerStatus &status);
    void connectionError(const QString &message);

private:
    void post(const QString &playerId, const QStringList &command);

    QNetworkAccessManager *m_network = nullptr;
    QString m_host;
    quint16 m_port = 9000;
    QString m_playerId;
};

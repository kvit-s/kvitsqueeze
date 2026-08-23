// SPDX-License-Identifier: MPL-2.0

#include "diagnosticsmodel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QSysInfo>

namespace {

// Enough to cover the interaction that went wrong plus what led to it, and
// small enough that the panel stays scrollable and the buffer stays cheap.
constexpr int kCapacity = 2000;

// One line of a JSON-RPC body can be a whole browse reply. The panel is for
// reading traffic, not for holding a copy of the library.
constexpr int kMaxLineLength = 2000;

QString sourceName(DiagnosticsModel::Source source)
{
    switch (source) {
    case DiagnosticsModel::ControlOut: return QStringLiteral("→ server");
    case DiagnosticsModel::ControlIn:  return QStringLiteral("← server");
    case DiagnosticsModel::Engine:     return QStringLiteral("engine");
    case DiagnosticsModel::App:        return QStringLiteral("app");
    }
    return {};
}

} // namespace

DiagnosticsModel::DiagnosticsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DiagnosticsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_entries.size());
}

QVariant DiagnosticsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (static_cast<Roles>(role)) {
    case TextRole:       return entry.text;
    case SourceRole:     return int(entry.source);
    case SourceNameRole: return sourceName(entry.source);
    case TimestampRole:
        return QDateTime::fromMSecsSinceEpoch(entry.msecs).toString(QStringLiteral("HH:mm:ss.zzz"));
    }
    return {};
}

QHash<int, QByteArray> DiagnosticsModel::roleNames() const
{
    return {
        { TextRole,       "text" },
        { SourceRole,     "source" },
        { SourceNameRole, "sourceName" },
        { TimestampRole,  "timestamp" },
    };
}

void DiagnosticsModel::setPaused(bool paused)
{
    if (paused == m_paused)
        return;
    m_paused = paused;
    Q_EMIT pausedChanged();
}

void DiagnosticsModel::append(int source, const QString &text)
{
    if (m_paused || text.isEmpty())
        return;

    QString line = text;
    if (line.size() > kMaxLineLength)
        line = line.left(kMaxLineLength) + QStringLiteral("… (%1 more)")
                                               .arg(text.size() - kMaxLineLength);

    if (m_entries.size() >= kCapacity) {
        beginRemoveRows({}, 0, 0);
        m_entries.removeFirst();
        endRemoveRows();
    }

    const int row = static_cast<int>(m_entries.size());
    beginInsertRows({}, row, row);
    m_entries.append({ line, static_cast<Source>(source),
                       QDateTime::currentMSecsSinceEpoch() });
    endInsertRows();
    Q_EMIT countChanged();
}

void DiagnosticsModel::appendControl(bool outgoing, const QString &text)
{
    append(outgoing ? ControlOut : ControlIn, text);
}

void DiagnosticsModel::appendEngine(const QString &text)
{
    append(Engine, text);
}

void DiagnosticsModel::clear()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
    Q_EMIT countChanged();
}

QString DiagnosticsModel::asText() const
{
    QString text;
    text += QStringLiteral("SqeezeAmp %1\n").arg(QCoreApplication::applicationVersion());
    text += QStringLiteral("%1 %2 (%3)\n").arg(QSysInfo::prettyProductName(),
                                               QSysInfo::kernelVersion(),
                                               QSysInfo::currentCpuArchitecture());
    text += QStringLiteral("Qt %1\n\n").arg(QLatin1String(qVersion()));

    for (const Entry &entry : m_entries) {
        text += QDateTime::fromMSecsSinceEpoch(entry.msecs)
                    .toString(QStringLiteral("HH:mm:ss.zzz"));
        text += QLatin1Char(' ');
        text += sourceName(entry.source);
        text += QStringLiteral(": ");
        text += entry.text;
        text += QLatin1Char('\n');
    }
    return text;
}

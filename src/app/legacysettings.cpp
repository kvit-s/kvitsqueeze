// SPDX-License-Identifier: MPL-2.0

#include "legacysettings.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QSettings>

Q_LOGGING_CATEGORY(lcLegacySettings, "sqz.legacysettings")

namespace {
// What the organisation and application were called before the rename. Both
// were the same string, which is why the registry path repeats it.
constexpr auto kLegacyName = "SqeezeAmp";
} // namespace

namespace LegacySettings {

bool migrateIfNeeded()
{
    QSettings current;
    if (!current.allKeys().isEmpty())
        return false; // already has its own settings

    QSettings legacy(QSettings::NativeFormat, QSettings::UserScope,
                     QLatin1String(kLegacyName), QLatin1String(kLegacyName));
    const QStringList keys = legacy.allKeys();
    if (keys.isEmpty())
        return false; // nothing to come from — a clean install

    for (const QString &key : keys)
        current.setValue(key, legacy.value(key));
    current.sync();

    if (current.status() != QSettings::NoError) {
        // Not fatal. A failed copy means the app starts as a fresh install,
        // which is recoverable by typing the server address again; refusing to
        // start would not be.
        qCWarning(lcLegacySettings, "could not copy the settings from %s (status %d)",
                  kLegacyName, static_cast<int>(current.status()));
        return false;
    }

    qCInfo(lcLegacySettings, "copied %lld settings from the previous name (%s)",
           static_cast<long long>(keys.size()), kLegacyName);
    return true;
}

} // namespace LegacySettings

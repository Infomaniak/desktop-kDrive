/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "db/parmsdb.h"
#include "libcommon/utility/types.h"
#include "keychainmanager/apitoken.h"

#include <QSettings>
#include <QStringList>
#include <QDir>

namespace KDC {

class MigrationParams {
    public:
        MigrationParams();

        ExitCode migrateGeneralParams();
        ExitCode migrateAccountsParams();
        ExitCode migrateTemplateExclusion();
#if defined(KD_MACOS)
        ExitCode migrateAppExclusion();
#endif
        ExitCode migrateProxySettings(ProxyConfig &proxyConfig);
        ExitCode migrateSelectiveSyncs();

        ExitCode getOldAppPwd(const std::string &keychainKey, std::string &appPassword, bool &found);
        ExitCode setToken(User &user, const std::string &appPassword);

        void deleteUselessConfigFiles();
        void migrateGeometry(std::shared_ptr<std::vector<char>> &geometryStr);
        inline const std::unordered_map<int, std::pair<SyncPath, SyncName>> &getSyncToMigrate() { return _syncToMigrate; }
        static QDir configDir();
        static QString configFileName();
        bool isProxyNotSupported() const { return _proxyNotSupported; }

    private:
        ExitCode loadAccount(QSettings &settings);

        Language strToLanguage(QString lang);
        LogLevel intToLogLevel(int log);
        VirtualFileMode modeFromString(const QString &str);
        int extractDriveIdFromUrl(std::string url);
        QString excludeTemplatesFileName() const;
        QString excludeAppsFileName() const;
        ExitCode getTokenFromAppPassword(const std::string &email, const std::string &appPassword, ApiToken &apiToken);
        void setProxyNotSupported(bool proxyNotSupported) { _proxyNotSupported = proxyNotSupported; }

        log4cplus::Logger _logger;
        std::unordered_map<int, std::pair<SyncPath, SyncName>> _syncToMigrate;
        bool _proxyNotSupported;
};

} // namespace KDC

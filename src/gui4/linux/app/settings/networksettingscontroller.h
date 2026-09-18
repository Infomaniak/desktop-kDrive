/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "app/services/parametersservice.h"
#include "libcommon/info/proxyconfiginfo.h"

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

#include <cstdint>
#include <optional>

namespace KDC {

class ParametersStore;
class TranslationService;

/** Owns the Network Settings proxy draft, connectivity check, and confirmed-snapshot persistence workflow. */
class NetworkSettingsController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool ready READ ready NOTIFY changed)
        Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
        Q_PROPERTY(bool valid READ valid NOTIFY changed)
        Q_PROPERTY(bool checking READ checking NOTIFY changed)
        Q_PROPERTY(bool saving READ saving NOTIFY changed)
        Q_PROPERTY(bool manual READ manual NOTIFY changed)
        Q_PROPERTY(int32_t proxyType READ proxyType NOTIFY changed)
        Q_PROPERTY(int32_t systemProxyType READ systemProxyType CONSTANT)
        Q_PROPERTY(int32_t manualProxyType READ manualProxyType CONSTANT)
        Q_PROPERTY(int32_t noProxyType READ noProxyType CONSTANT)
        Q_PROPERTY(QString hostName READ hostName NOTIFY changed)
        Q_PROPERTY(QString portText READ portText NOTIFY changed)
        Q_PROPERTY(bool needsAuth READ needsAuth NOTIFY changed)
        Q_PROPERTY(QString user READ user NOTIFY changed)
        Q_PROPERTY(QString password READ password NOTIFY changed)
        Q_PROPERTY(QString errorText READ errorText NOTIFY changed)

    public:
        NetworkSettingsController(ParametersStore &parametersStore, ParametersService &parametersService,
                                  const TranslationService &translationService, QObject *parent = nullptr);

        [[nodiscard]] bool ready() const;
        [[nodiscard]] bool dirty() const;
        [[nodiscard]] bool valid() const;
        [[nodiscard]] bool checking() const { return _checking; }
        [[nodiscard]] bool saving() const { return _saving; }
        [[nodiscard]] bool manual() const { return _proxyType == ProxyType::HTTP; }
        [[nodiscard]] int32_t proxyType() const { return static_cast<int32_t>(_proxyType); }
        [[nodiscard]] static int32_t systemProxyType() { return static_cast<int32_t>(ProxyType::System); }
        [[nodiscard]] static int32_t manualProxyType() { return static_cast<int32_t>(ProxyType::HTTP); }
        [[nodiscard]] static int32_t noProxyType() { return static_cast<int32_t>(ProxyType::None); }
        [[nodiscard]] const QString &hostName() const { return _hostName; }
        [[nodiscard]] const QString &portText() const { return _portText; }
        [[nodiscard]] bool needsAuth() const { return _needsAuth; }
        [[nodiscard]] const QString &user() const { return _user; }
        [[nodiscard]] const QString &password() const { return _password; }
        [[nodiscard]] QString errorText() const;

        Q_INVOKABLE void beginEditing();
        Q_INVOKABLE void setProxyType(int32_t type);
        Q_INVOKABLE void setHostName(const QString &hostName);
        Q_INVOKABLE void setPortText(const QString &portText);
        Q_INVOKABLE void setNeedsAuth(bool needsAuth);
        Q_INVOKABLE void setUser(const QString &user);
        Q_INVOKABLE void setPassword(const QString &password);
        Q_INVOKABLE void saveManual();
        Q_INVOKABLE void saveManualWithoutCheck();
        Q_INVOKABLE void dismissConnectionFailure();
        Q_INVOKABLE void cancelConnectionCheck();

    signals:
        void changed();
        void proxyConnectionFailureRequested();
        void saveFinished(bool success);

    private:
        enum class SaveKind : uint8_t {
            Immediate,
            Manual,
        };

        static ProxyType supportedType(ProxyType type);
        void loadConfirmed(bool preserveManualDraft = false);
        void saveImmediateType(ProxyType type);
        void saveConfig(const ProxyConfigInfo &config, SaveKind kind);
        void finishSave(const ExitInfo &result);
        void finishConnectionCheckFailure();
        [[nodiscard]] std::optional<ProxyConfigInfo> validatedManualConfig() const;

        ParametersStore &_parametersStore;
        ParametersService &_parametersService;
        ProxyConfigInfo _confirmedConfig;
        ProxyType _proxyType{ProxyType::None};
        QString _hostName;
        QString _portText;
        bool _needsAuth{false};
        QString _user;
        QString _password;
        bool _initialized{false};
        bool _checking{false};
        bool _saving{false};
        bool _saveFailed{false};
        SaveKind _saveKind{SaveKind::Manual};
        std::optional<ProxyConfigInfo> _pendingManualConfig;
        QTcpSocket _socket;
        QTimer _timeout;
};

} // namespace KDC

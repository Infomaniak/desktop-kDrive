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

#include "networksettingscontroller.h"

#include "app/cache/parametersstore.h"
#include "app/services/translationservice.h"

#include <QAbstractSocket>
#include <QLoggingCategory>
#include <QNetworkProxy>
#include <QPointer>

#include <limits>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {
constexpr uint16_t defaultProxyPort = 8080;
constexpr uint16_t minimumProxyPort = 1;
constexpr uint32_t connectionTimeoutMs = 5000;
constexpr qsizetype maximumHostLength = 200;
constexpr uint16_t connectionCheckPort = 443;
const auto connectionCheckHost = u"api.infomaniak.com"_s;

Q_LOGGING_CATEGORY(lcNetworkSettings, "gui.v4.settings.network", QtInfoMsg)
} // namespace

NetworkSettingsController::NetworkSettingsController(ParametersStore &parametersStore, ParametersService &parametersService,
                                                     const TranslationService &translationService, QObject *const parent) :
    QObject(parent),
    _parametersStore(parametersStore),
    _parametersService(parametersService) {
    _timeout.setSingleShot(true);
    _timeout.setInterval(connectionTimeoutMs);

    (void) connect(&_parametersStore, &ParametersStore::parametersInfoChanged, this, [this] {
        if (_saving) {
            return;
        }
        loadConfirmed(_initialized && (dirty() || !manual()));
    });
    (void) connect(&_socket, &QTcpSocket::connected, this, [this] {
        if (!_checking || !_pendingManualConfig) {
            return;
        }

        _checking = false;
        _timeout.stop();
        _socket.abort();
        const auto config = *_pendingManualConfig;
        emit changed();
        saveConfig(config, SaveKind::Manual);
    });
    (void) connect(&_socket, &QTcpSocket::errorOccurred, this,
                   [this](const QAbstractSocket::SocketError) { finishConnectionCheckFailure(); });
    (void) connect(&_timeout, &QTimer::timeout, this, &NetworkSettingsController::finishConnectionCheckFailure);
    (void) connect(&translationService, &TranslationService::languageChanged, this, &NetworkSettingsController::changed);
}

bool NetworkSettingsController::ready() const {
    return _initialized && _parametersStore.parametersInfo().has_value();
}

bool NetworkSettingsController::dirty() const {
    if (!_initialized || _proxyType != supportedType(_confirmedConfig.type())) {
        return _initialized;
    }
    if (!manual()) {
        return false;
    }

    const auto config = validatedManualConfig();
    return !config || *config != _confirmedConfig;
}

bool NetworkSettingsController::valid() const {
    return validatedManualConfig().has_value();
}

QString NetworkSettingsController::errorText() const {
    return _saveFailed ? qtTrId("linuxSettingsSaveError") : QString{};
}

void NetworkSettingsController::beginEditing() {
    if (_checking || _saving) {
        return;
    }
    loadConfirmed();
}

void NetworkSettingsController::setProxyType(const int32_t type) {
    const auto requestedType = static_cast<ProxyType>(type);
    if (requestedType != ProxyType::System && requestedType != ProxyType::HTTP && requestedType != ProxyType::None) {
        qCWarning(lcNetworkSettings) << u"Ignoring unsupported proxy type selection"_s << type;
        return;
    }
    if (!ready() || _checking || _saving || requestedType == _proxyType) {
        return;
    }

    _saveFailed = false;
    _proxyType = requestedType;
    if (manual()) {
        if (_portText.isEmpty()) {
            _portText = QString::number(defaultProxyPort);
        }
        emit changed();
        return;
    }

    emit changed();
    saveImmediateType(requestedType);
}

void NetworkSettingsController::setHostName(const QString &hostName) {
    if (_checking || _saving || hostName == _hostName) {
        return;
    }
    _hostName = hostName;
    _saveFailed = false;
    emit changed();
}

void NetworkSettingsController::setPortText(const QString &portText) {
    if (_checking || _saving || portText == _portText) {
        return;
    }
    _portText = portText;
    _saveFailed = false;
    emit changed();
}

void NetworkSettingsController::setNeedsAuth(const bool needsAuth) {
    if (_checking || _saving || needsAuth == _needsAuth) {
        return;
    }
    _needsAuth = needsAuth;
    _saveFailed = false;
    emit changed();
}

void NetworkSettingsController::setUser(const QString &user) {
    if (_checking || _saving || user == _user) {
        return;
    }
    _user = user;
    _saveFailed = false;
    emit changed();
}

void NetworkSettingsController::setPassword(const QString &password) {
    if (_checking || _saving || password == _password) {
        return;
    }
    _password = password;
    _saveFailed = false;
    emit changed();
}

void NetworkSettingsController::saveManual() {
    if (_checking || _saving) {
        return;
    }

    _pendingManualConfig = validatedManualConfig();
    if (!_pendingManualConfig) {
        return;
    }

    _saveFailed = false;
    _checking = true;
    emit changed();

    const auto &config = *_pendingManualConfig;
    QNetworkProxy proxy{QNetworkProxy::HttpProxy, config.hostName(), static_cast<quint16>(config.port())};
    if (config.needsAuth()) {
        proxy.setUser(config.user());
        proxy.setPassword(config.pwd());
    }
    _socket.setProxy(proxy);

    _timeout.start();
    _socket.connectToHost(connectionCheckHost, connectionCheckPort);
}

void NetworkSettingsController::saveManualWithoutCheck() {
    if (_checking || _saving || !_pendingManualConfig) {
        return;
    }
    saveConfig(*_pendingManualConfig, SaveKind::Manual);
}

void NetworkSettingsController::dismissConnectionFailure() {
    if (_saving) {
        return;
    }
    _pendingManualConfig.reset();
}

void NetworkSettingsController::cancelConnectionCheck() {
    if (!_checking) {
        return;
    }
    _checking = false;
    _timeout.stop();
    _socket.abort();
    _pendingManualConfig.reset();
    emit changed();
}

ProxyType NetworkSettingsController::supportedType(const ProxyType type) {
    switch (type) {
        case ProxyType::System:
        case ProxyType::HTTP:
        case ProxyType::None:
            return type;
        default:
            qCWarning(lcNetworkSettings) << u"Received unsupported proxy type"_s << static_cast<int32_t>(type)
                                         << u"; using direct connection in Network Settings"_s;
            return ProxyType::None;
    }
}

void NetworkSettingsController::loadConfirmed(const bool preserveManualDraft) {
    const auto parameters = _parametersStore.parametersInfo();
    if (!parameters) {
        _initialized = false;
        emit changed();
        return;
    }

    const auto config = parameters->proxyConfigInfo();
    const auto type = supportedType(config.type());
    _confirmedConfig = config;
    _confirmedConfig.setType(type);
    _initialized = true;
    _saveFailed = false;

    if (!preserveManualDraft) {
        _proxyType = type;
        _hostName = config.hostName();
        _portText = config.port() > 0 ? QString::number(config.port()) : QString{};
        _needsAuth = config.needsAuth();
        _user = config.user();
        _password = config.pwd();
    }
    emit changed();
}

void NetworkSettingsController::saveImmediateType(const ProxyType type) {
    _saveKind = SaveKind::Immediate;
    _saving = true;
    _saveFailed = false;
    emit changed();

    _parametersService.updateParameters(
            [type](ParametersInfo &parameters) {
                auto config = parameters.proxyConfigInfo();
                config.setType(type);
                parameters.setProxyConfigInfo(config);
            },
            [self = QPointer(this)](const ExitInfo &result) {
                if (self) {
                    self->finishSave(result);
                }
            });
}

void NetworkSettingsController::saveConfig(const ProxyConfigInfo &config, const SaveKind kind) {
    _saveKind = kind;
    _saving = true;
    _saveFailed = false;
    emit changed();

    _parametersService.updateParameters([config](ParametersInfo &parameters) { parameters.setProxyConfigInfo(config); },
                                        [self = QPointer(this)](const ExitInfo &result) {
                                            if (self) {
                                                self->finishSave(result);
                                            }
                                        });
}

void NetworkSettingsController::finishSave(const ExitInfo &result) {
    _saving = false;
    _saveFailed = !result;

    if (result) {
        loadConfirmed(_saveKind == SaveKind::Immediate);
    } else if (_saveKind == SaveKind::Immediate) {
        _proxyType = supportedType(_confirmedConfig.type());
        emit changed();
    } else {
        emit changed();
    }

    _pendingManualConfig.reset();
    emit saveFinished(result);
}

void NetworkSettingsController::finishConnectionCheckFailure() {
    if (!_checking) {
        return;
    }

    _checking = false;
    _timeout.stop();
    _socket.abort();
    emit changed();
    emit proxyConnectionFailureRequested();
}

std::optional<ProxyConfigInfo> NetworkSettingsController::validatedManualConfig() const {
    if (!ready() || !manual()) {
        return std::nullopt;
    }

    const auto hostName = _hostName.trimmed();
    bool digitsOnly = !_portText.isEmpty();
    for (const auto character: _portText) {
        if (character < u'0' || character > u'9') {
            digitsOnly = false;
            break;
        }
    }
    bool portOk = false;
    const auto port = _portText.toUInt(&portOk);
    if (hostName.isEmpty() || hostName.size() > maximumHostLength || !digitsOnly || !portOk || port < minimumProxyPort ||
        port > std::numeric_limits<uint16_t>::max() || (_needsAuth && (_user.isEmpty() || _password.isEmpty()))) {
        return std::nullopt;
    }

    return ProxyConfigInfo{ProxyType::HTTP,
                           hostName,
                           static_cast<int>(port),
                           _needsAuth,
                           _needsAuth ? _user : QString{},
                           _needsAuth ? _password : QString{}};
}

} // namespace KDC

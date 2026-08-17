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

#include "data/proxyconfig.h"

#include "utility/utility.h"

namespace KDC {

static const auto proxyConfigInfoType = "type";
static const auto proxyConfigInfoHostName = "hostName";
static const auto proxyConfigInfoPort = "port";
static const auto proxyConfigInfoNeedsAuth = "needsAuth";
static const auto proxyConfigInfoUser = "user";
static const auto proxyConfigInfoPwd = "pwd";

ProxyConfig::ProxyConfig(const ProxyType type, const std::string &hostName, const int port, const bool needsAuth,
                         const std::string &user, const std::string &pwd) :
    _type(type),
    _hostName(hostName),
    _port(port),
    _needsAuth(needsAuth),
    _user(user),
    _pwd(pwd) {}

void ProxyConfig::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoType, _type);
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoHostName, _hostName);
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoPort, _port);
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoNeedsAuth, _needsAuth);
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoUser, _user);
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoPwd, _pwd);
}

void ProxyConfig::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoType, _type);
    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoHostName, _hostName);
    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoPort, _port);
    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoNeedsAuth, _needsAuth);
    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoUser, _user);
    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoPwd, _pwd);
}

QDataStream &operator>>(QDataStream &in, ProxyConfig &proxyConfig) {
    QString hostName, user, pwd;
    in >> proxyConfig._type >> hostName >> proxyConfig._port >> proxyConfig._needsAuth >> user >> pwd;
    proxyConfig._hostName = hostName.toStdString();
    proxyConfig._user = user.toStdString();
    proxyConfig._pwd = pwd.toStdString();
    return in;
}

QDataStream &operator<<(QDataStream &out, const ProxyConfig &proxyConfig) {
    out << proxyConfig._type << QString::fromStdString(proxyConfig._hostName) << proxyConfig._port << proxyConfig._needsAuth
        << QString::fromStdString(proxyConfig._user) << QString::fromStdString(proxyConfig._pwd);
    return out;
}

} // namespace KDC

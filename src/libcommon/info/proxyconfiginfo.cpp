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

#include "info/proxyconfiginfo.h"

#include "utility/utility.h"

namespace KDC {

static const auto proxyConfigInfoType = "type";
static const auto proxyConfigInfoHostName = "hostName";
static const auto proxyConfigInfoPort = "port";
static const auto proxyConfigInfoNeedsAuth = "needsAuth";
static const auto proxyConfigInfoUser = "user";
static const auto proxyConfigInfoPwd = "pwd";

ProxyConfigInfo::ProxyConfigInfo(const ProxyType type, QString hostName, const int port, const bool needsAuth, QString user,
                                 QString pwd) :
    _type(type),
    _hostName(std::move(hostName)),
    _port(port),
    _needsAuth(needsAuth),
    _user(std::move(user)),
    _pwd(std::move(pwd)) {}

void ProxyConfigInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoType, _type);
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoHostName, CommonUtility::qStr2CommString(_hostName));
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoPort, _port);
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoNeedsAuth, _needsAuth);
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoUser, CommonUtility::qStr2CommString(_user));
    CommonUtility::writeValueToStruct(dstruct, proxyConfigInfoPwd, CommonUtility::qStr2CommString(_pwd));
}

void ProxyConfigInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoType, _type);

    CommString hostNameCommStr;
    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoHostName, hostNameCommStr);
    _hostName = CommonUtility::commString2QStr(hostNameCommStr);

    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoPort, _port);
    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoNeedsAuth, _needsAuth);

    CommString userCommStr;
    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoUser, userCommStr);
    _user = CommonUtility::commString2QStr(userCommStr);

    CommString pwdCommStr;
    CommonUtility::readValueFromStruct(dstruct, proxyConfigInfoPwd, pwdCommStr);
    _pwd = CommonUtility::commString2QStr(pwdCommStr);
}

QDataStream &operator>>(QDataStream &in, ProxyConfigInfo &proxyConfigInfo) {
    in >> proxyConfigInfo._type >> proxyConfigInfo._hostName >> proxyConfigInfo._port >> proxyConfigInfo._needsAuth >>
            proxyConfigInfo._user >> proxyConfigInfo._pwd;
    return in;
}

QDataStream &operator<<(QDataStream &out, const ProxyConfigInfo &proxyConfigInfo) {
    out << proxyConfigInfo._type << proxyConfigInfo._hostName << proxyConfigInfo._port << proxyConfigInfo._needsAuth
        << proxyConfigInfo._user << proxyConfigInfo._pwd;
    return out;
}

} // namespace KDC

/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "updateinfo.h"

#include <QString>

#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/Document.h>
#include <Poco/DOM/Element.h>
#include <Poco/DOM/Node.h>
#include <Poco/DOM/AutoPtr.h>

#include "libcommon/utility/types.h"
#include "libcommonserver/utility/utility.h"
#include <iostream>
#include <fstream>

namespace KDC {

void UpdateInfo::setVersion(const QString &v) {
    mVersion = v;
}

QString UpdateInfo::version() const {
    return mVersion;
}

void UpdateInfo::setVersionString(const QString &v) {
    mVersionString = v;
}

QString UpdateInfo::versionString() const {
    return mVersionString;
}

void UpdateInfo::setWeb(const QString &v) {
    mWeb = v;
}

QString UpdateInfo::web() const {
    return mWeb;
}

void UpdateInfo::setDownloadUrl(const QString &v) {
    mDownloadUrl = v;
}

QString UpdateInfo::downloadUrl() const {
    return mDownloadUrl;
}

UpdateInfo UpdateInfo::parseString(const QString &xml, bool *ok) {
    if (ok) {
        *ok = true;
    }
    Poco::AutoPtr<Poco::XML::Document> doc;
    try {
        Poco::XML::DOMParser parser;
        doc = parser.parseString(xml.toStdString());
    } catch (...) {
        if (ok) {
            *ok = false;
        }
        return UpdateInfo();
    }

    if (!doc) {
        if (ok) {
            *ok = false;
        }
        return UpdateInfo();
    }

    UpdateInfo c = UpdateInfo();
    std::string nodeText;

    // TODO : Replace QString and remove the fromStdString from setters below
    if (Utility::findNodeValue(*doc, "kdriveclient/version", &nodeText)) {
        c.setVersion(QString::fromStdString(nodeText));
    }
    if (Utility::findNodeValue(*doc, "kdriveclient/versionstring", &nodeText)) {
        c.setVersionString(QString::fromStdString(nodeText));
    }
    if (Utility::findNodeValue(*doc, "kdriveclient/web", &nodeText)) {
        c.setWeb(QString::fromStdString(nodeText));
    }
    if (Utility::findNodeValue(*doc, "kdriveclient/downloadurl", &nodeText)) {
        c.setDownloadUrl(QString::fromStdString(nodeText));
    }

    nodeText.clear();

    return c;
}

} // namespace KDC

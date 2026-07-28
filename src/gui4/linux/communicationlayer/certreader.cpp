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

#include "certreader.h"

#include "keychain/keychain.h"

#include <QByteArray>
#include <QLoggingCategory>

#include <utility>

Q_LOGGING_CATEGORY(lcCertReader, "gui.v4.certreader", QtInfoMsg)

namespace {
// Must match KeyChainStorage exactly, or getPassword returns NotFound.
const std::string keychainPackage("com.infomaniak.drive");
const std::string keychainService("desktopclient");
} // namespace

namespace KDC {

CertReader::CertReader(std::string keychainKey) :
    _keychainKey(std::move(keychainKey)) {}

std::string CertReader::readPem(bool &found) const {
    found = false;
    keychain::Error error{};
    const std::string pem = keychain::getPassword(keychainPackage, keychainService, _keychainKey, error);

    if (error.type == keychain::ErrorType::NotFound) {
        // Entry not present yet — not an error; caller may retry.
        return {};
    }
    if (error) {
        qCWarning(lcCertReader) << "Keychain read failed:" << QString::fromStdString(error.message) << "(code:" << error.code
                                << ")";
        return {};
    }
    if (pem.empty()) {
        qCWarning(lcCertReader) << "Keychain returned an empty certificate";
        return {};
    }

    found = true;
    return pem;
}

QSslCertificate CertReader::readCertificate(bool &found) const {
    const std::string pem = readPem(found);
    if (!found || pem.empty()) {
        return QSslCertificate();
    }

    const QSslCertificate cert(QByteArray::fromStdString(pem), QSsl::Pem);
    if (cert.isNull()) {
        qCWarning(lcCertReader) << "Failed to parse certificate PEM from keychain";
    } else {
        qCInfo(lcCertReader) << "Certificate loaded from keychain";
    }
    return cert;
}

} // namespace KDC

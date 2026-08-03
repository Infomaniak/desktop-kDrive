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

#include "comm.h"
#include "keychain/keychain.h"

#include <QByteArray>
#include <QLoggingCategory>

#include <utility>

Q_LOGGING_CATEGORY(lcCertReader, "gui.v4.certreader", QtInfoMsg)

namespace KDC {

CertReader::CertReader(std::string keychainKey) :
    _keychainKey(std::move(keychainKey)) {}

bool CertReader::readPem(std::string &outPem) const {
    keychain::Error error{};
    outPem = keychain::getPassword(std::string(package), std::string(service), _keychainKey, error);

    if (error.type == keychain::ErrorType::NotFound) {
        // Entry not present yet, not an error; caller may retry.
        return false;
    }
    if (error) {
        qCWarning(lcCertReader) << "Keychain read failed:" << QString::fromStdString(error.message) << "(code:" << error.code
                                << ")";
        return false;
    }
    if (outPem.empty()) {
        qCWarning(lcCertReader) << "Keychain returned an empty certificate";
        return false;
    }
    return true;
}

bool CertReader::readCertificate(QSslCertificate &certificate) const {
    std::string pem;
    if (!readPem(pem)) {
        return false;
    }

    certificate = QSslCertificate(QByteArray::fromStdString(pem), QSsl::Pem);
    if (certificate.isNull()) {
        qCWarning(lcCertReader) << "Failed to parse certificate PEM from keychain";
        return false;
    }
    qCInfo(lcCertReader) << "Certificate loaded from keychain";
    return true;
}

} // namespace KDC

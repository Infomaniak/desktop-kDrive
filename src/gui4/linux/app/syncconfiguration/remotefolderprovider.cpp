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

#include "remotefolderprovider.h"

#include "app/services/commservice.h"

#include <QLocale>

namespace KDC {

namespace RemoteFolders {

namespace {
// The server names these folders in English whatever the language of the drive (`ApiTranslator::v3SpecialFolderNames`).
constexpr QStringView commonDocumentsPath = u"/Common documents";
constexpr QStringView sharedPath = u"/Shared";
} // namespace

FolderKind folderKind(const QString &drivePath) {
    const QString normalizedPath = drivePath.normalized(QString::NormalizationForm_C);

    if (normalizedPath == commonDocumentsPath) {
        return FolderKind::CommonDocuments;
    }

    if (normalizedPath == sharedPath) {
        return FolderKind::Shared;
    }

    return FolderKind::Regular;
}

// Folder names are user data rather than translated UI text: their order follows the system locale, so a runtime
// application-language change cannot give already loaded and newly loaded branches different collations.
SiblingOrder::SiblingOrder() :
    _collator(QLocale::system()) {
    // Numbers compare by value, as in file managers: "2" comes before "10".
    _collator.setNumericMode(true);
}

bool SiblingOrder::lessThan(const FolderKind lhsKind, const QString &lhsName, const FolderKind rhsKind,
                            const QString &rhsName) const {
    if (lhsKind != rhsKind) {
        return lhsKind < rhsKind;
    }

    return _collator.compare(lhsName, rhsName) < 0;
}

} // namespace RemoteFolders

CommRemoteFolderProvider::CommRemoteFolderProvider(CommService &commService) :
    _commService(commService) {}

void CommRemoteFolderProvider::requestNodeInfo(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId,
                                               const NodeInfoCallback &callback) const {
    _commService.requestNodeInfo(userDbId, driveId, nodeId, true, callback);
}

void CommRemoteFolderProvider::requestChildren(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId,
                                               const ChildrenCallback &callback) const {
    _commService.requestNodeSubfolders(userDbId, driveId, nodeId, true, callback);
}

void CommRemoteFolderProvider::requestSize(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId,
                                           const SizeCallback &callback) const {
    _commService.requestNodeFolderSize(userDbId, driveId, nodeId, callback);
}

} // namespace KDC

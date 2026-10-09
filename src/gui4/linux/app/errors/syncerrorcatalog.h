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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "app/errors/syncerrorkind.h"
#include "libcommon/data/error.h"

namespace KDC {

// Properties of an error kind that do not depend on the error or on the user.
struct SyncErrorTraits {
        SyncErrorCategory category{SyncErrorCategory::SystemAndPermissions};
        SyncErrorRowVariant variant{SyncErrorRowVariant::Standard};
        SyncErrorExplanation explanation{SyncErrorExplanation::None};
        bool showsInTray{false};
};

// Translation id of a text, translated by QML with `qsTrId`, and the number of `%n` placeholders it expects. All of them
// are filled with the translated node label (`syncErrorNodeLabelId`). A translation starting with the node label
// ("%1 name too long") gets a capital first letter; other texts are kept as is, so that "kDrive" keeps its lowercase "k".
struct SyncErrorText {
        const char *id{nullptr};
        uint8_t nodeLabelArgs{0};
};

// Texts and action of an error row. A text without id, such as the label of a missing action, is empty.
struct SyncErrorPresentation {
        SyncErrorText title;
        SyncErrorText description;
        SyncErrorAction action{SyncErrorAction::None};
        SyncErrorText actionLabel;
};

[[nodiscard]] SyncErrorTraits syncErrorTraits(SyncErrorKind kind);

// Section of the errors page for an error. A user-resolvable conflict is always a version conflict, whatever its kind.
[[nodiscard]] SyncErrorCategory categorizeSyncError(const Error &error, SyncErrorKind kind);

// Some descriptions and actions depend on the admin rights of the user on the drive.
[[nodiscard]] SyncErrorPresentation describeSyncError(SyncErrorKind kind, bool isAdmin);

// Translation id of the node label ("file" or "folder"), or `nullptr` when the node type is unknown.
[[nodiscard]] const char *syncErrorNodeLabelId(NodeType nodeType);

[[nodiscard]] const char *syncErrorCategoryTitleId(SyncErrorCategory category);

} // namespace KDC

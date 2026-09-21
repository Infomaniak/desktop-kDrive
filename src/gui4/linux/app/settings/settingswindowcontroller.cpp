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

#include "settingswindowcontroller.h"

namespace KDC {

SettingsWindowController::SettingsWindowController(ParametersStore &parametersStore, ParametersService &parametersService,
                                                   TranslationService &translationService,
                                                   UpdateStatusService &updateStatusService, QObject *const parent) :
    QObject(parent),
    _general(parametersStore, parametersService, translationService, updateStatusService, this) {}

void SettingsWindowController::refreshUpdates() const {
    _general.refreshUpdates();
}

} // namespace KDC

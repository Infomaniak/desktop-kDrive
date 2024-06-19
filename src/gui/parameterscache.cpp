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

#include "parameterscache.h"
#include "guirequests.h"
#include "custommessagebox.h"

#include <QLoggingCategory>

namespace KDC {

Q_LOGGING_CATEGORY(lcAppParameters, "gui.appparameters", QtInfoMsg)

std::shared_ptr<ParametersCache> ParametersCache::_instance = nullptr;

std::shared_ptr<ParametersCache> ParametersCache::instance() noexcept {
    if (_instance == nullptr) {
        try {
            _instance = std::shared_ptr<ParametersCache>(new ParametersCache());
        } catch (...) {
            return nullptr;
        }
    }

    return _instance;
}

ParametersCache::ParametersCache() {
    // Load parameters
    const ExitCode exitCode = GuiRequests::getParameters(_parametersInfo);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcAppParameters()) << "Error in Requests::getParameters : " << enumClassToInt(exitCode);
        throw std::runtime_error("Failed to create ParametersCache instance!");
    }
}

bool ParametersCache::saveParametersInfo(bool displayMessageBoxOnError) {
    if (const ExitCode exitCode = GuiRequests::updateParameters(_parametersInfo); exitCode != ExitCode::Ok) {
        qCWarning(lcAppParameters()) << "Error in Requests::updateParameters";
        if (displayMessageBoxOnError) {
            CustomMessageBox msgBox(QMessageBox::Warning,
                                    exitCode == ExitCode::SystemError
                                        ? QObject::tr("Unable to save parameters, please retry later.")
                                        : QObject::tr("Unable to save parameters!"),
                                    QMessageBox::Ok);
            msgBox.exec();
        }
        return false;
    }

    return true;
}

}  // namespace KDC

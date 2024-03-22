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

#pragma once

#include "abstractfileitemwidget.h"

#include "synchronizeditem.h"
#include "customwordwraplabel.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/errorinfo.h"

#include <QLabel>
#include <QWidget>
#include <QBoxLayout>
#include <QListWidgetItem>


namespace KDC {

class ClientGui;

class GenericErrorItemWidget : public AbstractFileItemWidget {
        Q_OBJECT

    public:
        explicit GenericErrorItemWidget(std::shared_ptr<ClientGui> gui, const QString &errorMsg, const ErrorInfo &errorInfo,
                                        QWidget *parent = nullptr);

        virtual void init() override;

    protected slots:
        virtual void openFolder(const QString &path) override;

    private:
        bool openInWebview();

        std::shared_ptr<ClientGui> _gui;
        ErrorInfo _errorInfo;
        const QString _errorMsg;
};

}  // namespace KDC

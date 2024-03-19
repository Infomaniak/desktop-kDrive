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

#include "customdialog.h"
#include "customradiobutton.h"
#include "customcheckbox.h"
#include "customcombobox.h"
#include "libcommon/utility/types.h"
#include "libcommon/info/proxyconfiginfo.h"

#include <QBoxLayout>
#include <QIntValidator>
#include <QLineEdit>
#include <QPushButton>

namespace KDC {

class ProxyServerDialog : public CustomDialog {
        Q_OBJECT

    public:
        explicit ProxyServerDialog(QWidget *parent = nullptr);

    private:
        static std::map<ProxyType, std::pair<int, QString>> _manualProxyMap;

        class PortValidator : public QIntValidator {
            public:
                PortValidator(QObject *parent = nullptr) : QIntValidator(0, 65535, parent) {}

            private:
                QValidator::State validate(QString &input, int &pos) const override {
                    QValidator::State state = QIntValidator::validate(input, pos);
                    return state == QValidator::Intermediate ? QValidator::Invalid : state;
                }
        };

        ProxyConfigInfo _proxyConfigInfo;
        CustomRadioButton *_noProxyButton;
        CustomRadioButton *_systemProxyButton;
        CustomRadioButton *_manualProxyButton;
        QWidget *_manualProxyWidget;
        CustomComboBox *_proxyTypeComboBox;
        QLineEdit *_portLineEdit;
        QLineEdit *_addressLineEdit;
        CustomCheckBox *_authenticationCheckBox;
        QWidget *_authenticationWidget;
        QLineEdit *_loginLineEdit;
        QLineEdit *_pwdLineEdit;
        QPushButton *_saveButton;
        bool _needToSave;
        PortValidator *_portValidator;

        void initUI();
        void updateUI();
        void setNeedToSave(bool value);
        bool isSaveEnabled();
        void resetManualProxy();
        void resetAuthentication();

    private slots:
        void onExit();
        void onSaveButtonTriggered(bool checked = false);
        void onNoProxyButtonClicked(bool checked = false);
        void onSystemProxyButtonClicked(bool checked = false);
        void onManualProxyButtonClicked(bool checked = false);
        void onProxyTypeComboBoxActivated(int index);
        void onPortTextEdited(const QString &text);
        void onAddressTextEdited(const QString &text);
        void onAuthenticationCheckBoxClicked(bool checked = false);
        void onLoginTextEdited(const QString &text);
        void onPwdTextEdited(const QString &text);
};

}  // namespace KDC

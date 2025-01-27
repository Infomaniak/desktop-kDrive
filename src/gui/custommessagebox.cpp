/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "custommessagebox.h"
#include "guiutility.h"

#include <QColor>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>

namespace KDC {

static const int boxHMargin = 40;
static const int boxHSpacing = 10;
static const int messageVTMargin = 2;
static const int messageVBMargin = 15;

const std::string buttonTypeProperty = "buttonType";

Q_LOGGING_CATEGORY(lcCustomMessageBox, "gui.custommessagebox", QtInfoMsg)

CustomMessageBox::CustomMessageBox(QMessageBox::Icon icon, const QString &text, const QString &warningText, bool warning,
                                   QMessageBox::StandardButtons buttons, QWidget *parent) :
    CustomDialog(true, parent), _icon(icon), _warningLabel(nullptr), _textLabel(nullptr), _iconLabel(nullptr),
    _buttonsHBox(nullptr), _iconSize(QSize()), _buttonCount(0) {
    QVBoxLayout *mainLayout = this->mainLayout();

    // Warning text
    if (!warningText.isEmpty()) {
        QHBoxLayout *warningHBox = new QHBoxLayout();
        warningHBox->setContentsMargins(boxHMargin, messageVTMargin, boxHMargin, messageVBMargin);
        mainLayout->addLayout(warningHBox);

        _warningLabel = new QLabel(this);
        _warningLabel->setObjectName(warning ? "warningTextLabel" : "okTextLabel");
        _warningLabel->setText(warningText);
        _warningLabel->setWordWrap(true);
        _warningLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        warningHBox->addWidget(_warningLabel);
    }

    // Icon + text
    QHBoxLayout *messageHBox = new QHBoxLayout();
    messageHBox->setContentsMargins(boxHMargin, 0, boxHMargin, messageVBMargin);
    messageHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(messageHBox);

    QVBoxLayout *messageVBox = new QVBoxLayout();
    messageVBox->setContentsMargins(0, 0, 0, 0);
    messageHBox->addLayout(messageVBox);

    _iconLabel = new QLabel(this);
    messageVBox->addWidget(_iconLabel);
    messageVBox->setAlignment(Qt::AlignCenter);

    _textLabel = new QLabel(this);
    _textLabel->setObjectName("textLabel");
    _textLabel->setTextFormat(Qt::RichText);
    _textLabel->setOpenExternalLinks(true);
    _textLabel->setText(text);
    _textLabel->setWordWrap(true);
    _textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    messageHBox->addWidget(_textLabel);
    messageHBox->setStretchFactor(_textLabel, 1);

    mainLayout->addStretch();

    // Add dialog buttons
    _buttonsHBox = new QHBoxLayout();
    _buttonsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    _buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(_buttonsHBox);

    if (buttons & QMessageBox::Ok) {
        QPushButton *button = new QPushButton(this);
        button->setObjectName("nondefaultbutton");
        button->setFlat(true);
        button->setText(tr("OK"));
        _buttonsHBox->insertWidget(_buttonCount++, button);
        button->setProperty(buttonTypeProperty.c_str(), QMessageBox::Ok);
        connect(button, &QPushButton::clicked, this, &CustomMessageBox::onButtonClicked);
    }

    if (buttons & QMessageBox::Cancel) {
        QPushButton *button = new QPushButton(this);
        button->setObjectName("nondefaultbutton");
        button->setFlat(true);
        button->setText(tr("CANCEL"));
        _buttonsHBox->insertWidget(_buttonCount++, button);
        button->setProperty(buttonTypeProperty.c_str(), QMessageBox::Cancel);
        connect(button, &QPushButton::clicked, this, &CustomMessageBox::onButtonClicked);
    }

    if (buttons & QMessageBox::Yes) {
        QPushButton *button = new QPushButton(this);
        button->setObjectName("nondefaultbutton");
        button->setFlat(true);
        button->setText(tr("YES"));
        _buttonsHBox->insertWidget(_buttonCount++, button);
        button->setProperty(buttonTypeProperty.c_str(), QMessageBox::Yes);
        connect(button, &QPushButton::clicked, this, &CustomMessageBox::onButtonClicked);
    }

    if (buttons & QMessageBox::No) {
        QPushButton *button = new QPushButton(this);
        button->setObjectName("nondefaultbutton");
        button->setFlat(true);
        button->setText(tr("NO"));
        _buttonsHBox->insertWidget(_buttonCount++, button);
        button->setProperty(buttonTypeProperty.c_str(), QMessageBox::No);
        connect(button, &QPushButton::clicked, this, &CustomMessageBox::onButtonClicked);
    }

    _buttonsHBox->addStretch();

    connect(this, &CustomDialog::exit, this, &CustomMessageBox::onExit);
}

CustomMessageBox::CustomMessageBox(QMessageBox::Icon icon, const QString &text, QMessageBox::StandardButtons buttons,
                                   QWidget *parent) : CustomMessageBox(icon, text, QString(), false, buttons, parent) {}

void CustomMessageBox::addButton(const QString &text, QMessageBox::StandardButton buttonType) {
    QPushButton *button = new QPushButton(this);
    button->setObjectName("nondefaultbutton");
    button->setFlat(true);
    button->setText(text);
    _buttonsHBox->insertWidget(_buttonCount++, button);
    button->setProperty(buttonTypeProperty.c_str(), buttonType);
    connect(button, &QPushButton::clicked, this, &CustomMessageBox::onButtonClicked);
}

void CustomMessageBox::setDefaultButton(QMessageBox::StandardButton buttonType) {
    QList<QPushButton *> buttonList = findChildren<QPushButton *>();
    for (QPushButton *button: buttonList) {
        QMessageBox::StandardButton currentButtonType =
                (QMessageBox::StandardButton) qvariant_cast<int>(button->property(buttonTypeProperty.c_str()));
        if (currentButtonType == buttonType) {
            button->setObjectName("defaultbutton");
            button->setDefault(true);
        }
    }
}

void CustomMessageBox::setIcon() {
    if (_iconSize != QSize()) {
        QString iconPath;
        QColor iconColor;
        switch (_icon) {
            case QMessageBox::NoIcon:
                return;
            case QMessageBox::Information:
                iconPath = ":/client/resources/icons/actions/information.svg";
                iconColor = QColor("#9F9F9F");
                break;
            case QMessageBox::Warning:
                iconPath = ":/client/resources/icons/actions/warning.svg";
                iconColor = QColor("#FF0000");
                break;
            case QMessageBox::Critical:
                iconPath = ":/client/resources/icons/actions/error-sync.svg";
                iconColor = QColor("#FF0000");
                break;
            case QMessageBox::Question:
                iconPath = ":/client/resources/icons/actions/help.svg";
                iconColor = QColor("#9F9F9F");
                break;
        }

        _iconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(iconPath, iconColor).pixmap(_iconSize));
    }
}

QSize CustomMessageBox::sizeHint() const {
    return QSize(
            contentsMargins().left() + contentsMargins().right() + 2 * boxHMargin + (_warningLabel ? _warningLabel->width() : 0),
            contentsMargins().top() + contentsMargins().bottom() + messageVTMargin + 2 * messageVBMargin +
                    (_warningLabel ? _warningLabel->height() : 0) + (_textLabel ? _textLabel->height() : 0) +
                    QPushButton().height());
}

void CustomMessageBox::showEvent(QShowEvent *event) {
    Q_UNUSED(event)
}

void CustomMessageBox::onButtonClicked(bool checked) {
    Q_UNUSED(checked)

    QMessageBox::StandardButton buttonType =
            (QMessageBox::StandardButton) qvariant_cast<int>(sender()->property(buttonTypeProperty.c_str()));
    done(buttonType);
}

void CustomMessageBox::onExit() {
    reject();
}

} // namespace KDC

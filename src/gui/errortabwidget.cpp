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

#include "errortabwidget.h"
#include "parametersdialog.h"
#include "gui/customtabbar.h"
#include "languagechangefilter.h"
#include "fixconflictingfilesdialog.h"
#include "menuwidgetlite.h"

#include <QVBoxLayout>
#include <QScrollBar>
#include <QMenu>

namespace KDC {

ErrorTabWidget::ErrorTabWidget(int driveDbId, bool generic, QWidget *parent)
    : QTabWidget(parent),
      _tabBar(nullptr),
      _paramsDialog((ParametersDialog *)parent),
      _autoResolvedErrorsListWidget(nullptr),
      _unresolvedErrorsListWidget(nullptr),
      _lastErrorTimestamp(0),
      _driveDbId(driveDbId),
      _generic(generic) {
    setObjectName("tabWidgetErrorWidget");
    _tabBar = new CustomTabBar(this);
    _tabBar->setObjectName("tabBarErrorWidget");
    setTabBar(_tabBar);
    QWidget *toResolve = new QWidget(this);
    addTab(toResolve, QString());

    // To resolve
    QVBoxLayout *vLayoutToRes = new QVBoxLayout();
    vLayoutToRes->setContentsMargins(0, 0, 0, 0);

    // line separator
    QFrame *line = new QFrame(this);
    line->setObjectName("line");
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    vLayoutToRes->addWidget(line);

    _resolveButton = new QPushButton(this);
    _resolveButton->setObjectName("resolveButton");
    _resolveButton->setVisible(false);
    MenuWidgetLite *menu = new MenuWidgetLite(_resolveButton, this);

    _resolveConflictsAction = new QAction(this);
    _resolveConflictsAction->setVisible(false);
    menu->addAction(_resolveConflictsAction);
    connect(_resolveConflictsAction, &QAction::triggered, this, &ErrorTabWidget::onResolveConflictErrors);

    _resolveUnsupportedCharactersAction = new QAction(this);
    _resolveUnsupportedCharactersAction->setVisible(false);
    menu->addAction(_resolveUnsupportedCharactersAction);
    connect(_resolveUnsupportedCharactersAction, &QAction::triggered, this,
            &ErrorTabWidget::onResolveUnsupportedCharactersErrors);

    _resolveButton->setMenu(menu);

    QHBoxLayout *hLayoutToRes = new QHBoxLayout();
    hLayoutToRes->setContentsMargins(0, 0, 0, 0);
    hLayoutToRes->setSpacing(2);
    _toResolveErrorsNb = new QLabel(this);
    _toResolveErrorsNb->setObjectName("nbErrorLabel");
    _toResolveErrorsLabel = new QLabel(this);
    _toResolveErrorsLabel->setObjectName("nbErrorLabel");
    hLayoutToRes->addWidget(_toResolveErrorsNb);
    hLayoutToRes->addWidget(_toResolveErrorsLabel);
    hLayoutToRes->addSpacing(10);
    hLayoutToRes->addWidget(_resolveButton);

    hLayoutToRes->addStretch();

    _clearToResButton = new CustomPushButton(":/client/resources/icons/actions/delete.svg", tr("Clear history"), this);
    _clearToResButton->setObjectName("clearHistoryButton");
    hLayoutToRes->addWidget(_clearToResButton);

    vLayoutToRes->addLayout(hLayoutToRes);
    vLayoutToRes->addStretch();

    // add QList of errors to vbox
    _unresolvedErrorsListWidget = new QListWidget(this);
    vLayoutToRes->addWidget(_unresolvedErrorsListWidget);
    vLayoutToRes->setStretchFactor(_unresolvedErrorsListWidget, 1);

    toResolve->setLayout(vLayoutToRes);

    _autoResolvedErrorsListWidget = new QListWidget(this);

    if (!generic) {
        QWidget *autoResolved = new QWidget(this);
        // Auto resolved
        addTab(autoResolved, QString());
        QVBoxLayout *vLayoutAutoRes = new QVBoxLayout();
        vLayoutAutoRes->setContentsMargins(0, 0, 0, 0);
        // line separator
        QFrame *line2 = new QFrame(this);
        line2->setObjectName("line");
        line2->setFrameShape(QFrame::HLine);
        line2->setFrameShadow(QFrame::Plain);
        vLayoutAutoRes->addWidget(line2);

        vLayoutAutoRes->addStretch();

        QHBoxLayout *hLayoutAutoRes = new QHBoxLayout();
        hLayoutAutoRes->setContentsMargins(0, 0, 0, 0);
        hLayoutAutoRes->setSpacing(2);
        _autoResolvedErrorsNb = new QLabel(this);
        _autoResolvedErrorsNb->setObjectName("nbErrorLabel");
        _autoResolvedErrorsLabel = new QLabel(this);
        _autoResolvedErrorsLabel->setObjectName("nbErrorLabel");
        hLayoutAutoRes->addWidget(_autoResolvedErrorsNb);
        hLayoutAutoRes->addWidget(_autoResolvedErrorsLabel);

        hLayoutAutoRes->addStretch();

        _clearAutoResButton = new CustomPushButton(":/client/resources/icons/actions/delete.svg", tr("Clear history"), this);
        _clearAutoResButton->setObjectName("clearHistoryButton");
        hLayoutAutoRes->addWidget(_clearAutoResButton);

        vLayoutAutoRes->addLayout(hLayoutAutoRes);

        vLayoutAutoRes->addWidget(_autoResolvedErrorsListWidget);
        vLayoutAutoRes->setStretchFactor(_autoResolvedErrorsListWidget, 1);
        autoResolved->setLayout(vLayoutAutoRes);

        vLayoutAutoRes->addStretch();
        connect(_clearAutoResButton, &CustomPushButton::clicked, this, &ErrorTabWidget::onClearAutoResErrorsClicked);
    }

    setUnresolvedErrorsCount(0);
    setAutoResolvedErrorsCount(0);

    LanguageChangeFilter *languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);

    connect(_clearToResButton, &CustomPushButton::clicked, this, &ErrorTabWidget::onClearToResErrorsClicked);
    connect(_unresolvedErrorsListWidget->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &ErrorTabWidget::onScrollBarValueChanged);
    connect(_autoResolvedErrorsListWidget->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &ErrorTabWidget::onScrollBarValueChanged);
}

void ErrorTabWidget::setUnresolvedErrorsCount(int count) {
    _tabBar->setUnResolvedNotifCount(count);
    _toResolveErrorsNb->setText(QString::number(count));
}

void ErrorTabWidget::setAutoResolvedErrorsCount(int count) {
    if (!_generic) {
        _autoResolvedErrorsNb->setText(QString::number(count));
    }
}

void ErrorTabWidget::retranslateUi() {
    int tabIndex = 0;

    setTabText(tabIndex, tr("To Resolve"));
    _toResolveErrorsLabel->setText(tr("problem(s) detected"));
    _clearToResButton->setText(tr("Clear history"));
    _resolveButton->setText(tr("Resolve"));
    _resolveConflictsAction->setText(tr("Conflicted item(s)"));
    _resolveUnsupportedCharactersAction->setText(tr("Item(s) with unsupported characters"));

    if (!_generic) {
        setTabText(++tabIndex, tr("Automatically resolved"));
        _autoResolvedErrorsLabel->setText(tr("problem(s) solved"));
        _clearAutoResButton->setText(tr("Clear history"));
    }
}

void ErrorTabWidget::showResolveConflicts(bool val) {
    if (_resolveButton && _resolveConflictsAction) {
        _resolveConflictsAction->setVisible(val);
        _resolveButton->setVisible(_resolveConflictsAction->isVisible() || _resolveUnsupportedCharactersAction->isVisible());
    }
}

void ErrorTabWidget::showResolveUnsupportedCharacters(bool) {
    // Never show Resolve button for unsupported characters, for now...
    // if (_resolveButton && _resolveUnsupportedCharactersAction) {
    //     _resolveUnsupportedCharactersAction->setVisible(val);
    //     _resolveButton->setVisible(_resolveConflictsAction->isVisible()
    //                                || _resolveUnsupportedCharactersAction->isVisible());
    // }
}

void ErrorTabWidget::onClearToResErrorsClicked() {
    emit _paramsDialog->clearErrors(_driveDbId, false);
}

void ErrorTabWidget::showEvent(QShowEvent *) {
    retranslateUi();
}

void ErrorTabWidget::onClearAutoResErrorsClicked() {
    emit _paramsDialog->clearErrors(_driveDbId, true);
}

void ErrorTabWidget::onScrollBarValueChanged(int value) {
    if (value != 0) {
        return;
    }
    _paramsDialog->onRefreshErrorList(_driveDbId);
}

void ErrorTabWidget::onResolveConflictErrors() {
    FixConflictingFilesDialog resolveDialog(_driveDbId, this);
    if (resolveDialog.exec() == QDialog::Accepted) {
        _paramsDialog->resolveConflictErrors(_driveDbId, resolveDialog.keepLocalVersion());
    }
}

void ErrorTabWidget::onResolveUnsupportedCharactersErrors() {
    _paramsDialog->resolveUnsupportedCharErrors(_driveDbId);
}

}  // namespace KDC

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

#pragma once

#include "libcommon/utility/types.h"

#include <QObject>
#include <QTranslator>
#include <QVariantList>

#include <optional>

class QQmlEngine;

namespace KDC {

class ParametersStore;

/** Owns the English fallback and selected locale, and announces changes to C++ presentation models. */
class TranslationService final : public QObject {
        Q_OBJECT

    public:
        explicit TranslationService(ParametersStore &parametersStore, QObject *parent = nullptr);
        void initialize();

        void setEngine(QQmlEngine *engine) { _qmlEngine = engine; }

        [[nodiscard]] static QVariantList languages();

        [[nodiscard]] Language language() const { return _currentLanguage.value_or(Language::Default); }

    signals:
        void languageChanged();

    private:
        void applyLanguage(Language language);

        ParametersStore &_parametersStore;
        QTranslator _baseTranslator;
        QTranslator _localizedTranslator;
        QQmlEngine *_qmlEngine{nullptr};
        std::optional<Language> _currentLanguage;
};

} // namespace KDC

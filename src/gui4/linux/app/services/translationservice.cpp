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

#include "translationservice.h"

#include "app/cache/parametersstore.h"
#include "libcommon/utility/utility.h"

#include <QCoreApplication>
#include <QLocale>
#include <QLoggingCategory>
#include <QQmlEngine>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {
Q_LOGGING_CATEGORY(lcTranslationService, "gui.v4.translations", QtInfoMsg)

// Returns the "Same as system" label in the system language, or English when unavailable. Evaluated once and cached.
QString systemLanguageLabel() {
    static const QString label = [] {
        constexpr auto translationId = QT_TRID_NOOP("labelSameAsSystem");
        QTranslator translator;
        (void) translator.load(QLocale::system(), u"client"_s, u"_"_s, u":/i18n"_s);

        if (const auto systemLabel = translator.translate(nullptr, translationId); !systemLabel.isEmpty()) {
            return systemLabel;
        }

        (void) translator.load(u"client_en"_s, u":/i18n"_s);
        return translator.translate(nullptr, translationId);
    }();

    return label;
}

// Use language autonyms rather than Qt's territory-specific locale names.
QString languageDisplayName(const Language language) {
    switch (language) {
        case Language::Default:
            return systemLanguageLabel();
        case Language::English:
            return u"English"_s;
        case Language::French:
            return u"Français"_s;
        case Language::German:
            return u"Deutsch"_s;
        case Language::Spanish:
            return u"Español"_s;
        case Language::Italian:
            return u"Italiano"_s;
        case Language::Dutch:
            return u"Nederlands"_s;
        case Language::Swedish:
            return u"Svenska"_s;
        case Language::Portuguese:
            return u"Português"_s;
        case Language::Polish:
            return u"Polski"_s;
        case Language::Norwegian:
            return u"Norsk"_s;
        case Language::Finnish:
            return u"Suomi"_s;
        case Language::Danish:
            return u"Dansk"_s;
        case Language::Greek:
            return u"Ελληνικά"_s;
        case Language::EnumEnd:
            return {};
    }

    return {};
}
} // namespace

TranslationService::TranslationService(ParametersStore &parametersStore, QObject *const parent) :
    QObject(parent),
    _parametersStore(parametersStore) {
    (void) connect(&parametersStore, &ParametersStore::parametersInfoChanged, this, [this] {
        if (const auto parametersInfo = _parametersStore.parametersInfo()) {
            applyLanguage(parametersInfo->language());
        }
    });
}

void TranslationService::initialize() {
    if (!_baseTranslator.load(u"client_en"_s, u":/i18n"_s) || !QCoreApplication::installTranslator(&_baseTranslator)) {
        qCWarning(lcTranslationService) << "English fallback catalog unavailable";
    }

    applyLanguage(Language::Default);
}

// Replace only the locale-specific catalog: untranslated entries still resolve through the English fallback.
void TranslationService::applyLanguage(const Language language) {
    if (_currentLanguage == language) {
        return;
    }

    (void) QCoreApplication::removeTranslator(&_localizedTranslator);

    const auto locale = language == Language::Default ? QLocale::system() : QLocale(CommonUtility::languageCode(language));

    QString warningMessage;
    if (!_localizedTranslator.load(locale, u"client"_s, u"_"_s, u":/i18n"_s)) {
        warningMessage = u"Localized translation catalog unavailable; using English fallback"_s;
    } else if (!QCoreApplication::installTranslator(&_localizedTranslator)) {
        warningMessage = u"Failed to install localized translation catalog"_s;
    }

    if (!warningMessage.isEmpty()) {
        qCWarning(lcTranslationService) << warningMessage << "| language:" << QString::fromStdString(toString(language))
                                        << "| locale:" << locale.name();
    }

    QLocale::setDefault(locale);
    _currentLanguage = language;

    if (_qmlEngine) {
        // The engine is intentionally null during the initial language application, as translations are installed before any QML
        // components are loaded.
        _qmlEngine->retranslate();
    }

    emit languageChanged();
}

QVariantList TranslationService::languages() {
    QVariantList availableLanguages;

    for (auto languageValue = static_cast<int32_t>(Language::Default); languageValue < static_cast<int32_t>(Language::EnumEnd);
         ++languageValue) {
        const auto language = static_cast<Language>(languageValue);
        const auto languageName = languageDisplayName(language);

        availableLanguages.append(QVariantMap{{u"value"_s, languageValue}, {u"label"_s, languageName}});
    }

    return availableLanguages;
}

} // namespace KDC

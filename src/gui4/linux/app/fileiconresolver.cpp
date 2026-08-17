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

#include "app/fileiconresolver.h"

#include <QFileInfo>
#include <QHash>
#include <QMimeDatabase>
#include <QMimeType>
#include <QSet>

// this let us use the u""_s instead of QStringLiteral("")
using namespace Qt::StringLiterals;

namespace KDC {

namespace {

// Safety net only. Callers drop the cache when the resolved set is replaced, so entries stay scoped to a single
// projection, that is one sync's activities plus its active errors.
constexpr qsizetype maxCacheEntries = 4096;

/* Narrow overrides for names the shared MIME database does not resolve, or resolves too generically. */
QString iconNameForFileName(const QString &fileName) {
    // clang-format off
    static const QHash<QString, QString> iconNamesByExtension{
            {u"m3u"_s, u"file-audio"_s},
            {u"m3u8"_s, u"file-audio"_s},

            {u"dot"_s, u"file-diagram"_s},
            {u"fig"_s, u"file-diagram"_s},
            {u"graffle"_s, u"file-diagram"_s},
            {u"jgraph"_s, u"file-diagram"_s},

            {u"otf"_s, u"file-font"_s},
            {u"pcf"_s, u"file-font"_s},

            {u"asm"_s, u"file-code"_s},
            {u"bash"_s, u"file-code"_s},
            {u"cjs"_s, u"file-code"_s},
            {u"jsx"_s, u"file-code"_s},
            {u"kts"_s, u"file-code"_s},
            {u"make"_s, u"file-code"_s},
            {u"pyw"_s, u"file-code"_s},
            {u"rs"_s, u"file-code"_s},
            {u"s"_s, u"file-code"_s},
            {u"sql"_s, u"file-code"_s},
            {u"swift"_s, u"file-code"_s},
            {u"ts"_s, u"file-code"_s},
            {u"tsx"_s, u"file-code"_s},
            {u"webmanifest"_s, u"file-code"_s},
            {u"zsh"_s, u"file-code"_s},

            {u"obj"_s, u"file-3d"_s},
            {u"step"_s, u"file-3d"_s},
            {u"stp"_s, u"file-3d"_s},
    };
    // clang-format on
    static const QSet textFileNames{
            u"changelog"_s,
            u"licence"_s,
            u"license"_s,
            u"readme"_s,
    };

    const QFileInfo fileInfo(fileName);
    if (const auto iconIt = iconNamesByExtension.constFind(fileInfo.suffix().toLower()); iconIt != iconNamesByExtension.cend()) {
        return *iconIt;
    }
    if (textFileNames.contains(fileInfo.fileName().toLower())) {
        return u"file-text"_s;
    }

    return {};
}

/* MIME types resolved by exact name, before any family or generic-icon rule. */
QString iconNameForExactMimeType(const QString &mimeName) {
    static const QHash<QString, QString> iconNamesByMimeType{
            {u"application/pdf"_s, u"file-pdf"_s},
            {u"application/x-bzpdf"_s, u"file-pdf"_s},
            {u"application/x-gzpdf"_s, u"file-pdf"_s},
            {u"application/x-lzpdf"_s, u"file-pdf"_s},
            {u"application/x-xzpdf"_s, u"file-pdf"_s},

            {u"application/vnd.apple.numbers"_s, u"file-grid"_s},
            {u"text/csv"_s, u"file-grid"_s},
            {u"text/tab-separated-values"_s, u"file-grid"_s},

            {u"application/vnd.apple.keynote"_s, u"file-chart"_s},

            {u"application/vnd.appimage"_s, u"file-zip"_s},
            {u"application/vnd.efi.iso"_s, u"file-zip"_s},
            {u"application/x-iso9660-appimage"_s, u"file-zip"_s},
    };

    const auto iconIt = iconNamesByMimeType.constFind(mimeName);
    return iconIt != iconNamesByMimeType.cend() ? *iconIt : QString{};
}

bool isDiagramMimeType(const QString &mimeName) {
    return mimeName.contains(u"visio"_s) || mimeName.contains(u"jgraph"_s) || mimeName.contains(u"omnigraffle"_s) ||
           mimeName.startsWith(u"application/x-dia"_s) || mimeName.startsWith(u"application/vnd.oasis.opendocument.graphics"_s) ||
           mimeName == u"application/vnd.sun.xml.draw"_s || mimeName == u"application/vnd.sun.xml.draw.template"_s ||
           mimeName == u"application/vnd.stardivision.draw"_s || mimeName == u"application/x-kivio"_s ||
           mimeName == u"text/vnd.graphviz"_s || mimeName == u"image/x-xfig"_s;
}

bool is3dMimeType(const QString &mimeName) {
    return mimeName.startsWith(u"model/"_s) || mimeName == u"application/x-blender"_s ||
           mimeName == u"application/x-kpovmodeler"_s;
}

bool isCodeMimeType(const QString &mimeName, const QString &genericIconName) {
    return genericIconName == u"text-x-script"_s || (mimeName.startsWith(u"text/x-"_s) && mimeName != u"text/x-log"_s) ||
           mimeName == u"text/html"_s || mimeName == u"text/css"_s || mimeName == u"application/xml"_s ||
           mimeName == u"application/xml-dtd"_s || mimeName == u"application/yaml"_s || mimeName == u"application/toml"_s ||
           mimeName.endsWith(u"+json"_s) || mimeName.endsWith(u"+xml"_s) || mimeName.endsWith(u"+yaml"_s);
}

QString iconNameForMimeType(const QMimeType &mimeType) {
    if (!mimeType.isValid() || mimeType.isDefault()) {
        return u"file"_s;
    }

    const QString mimeName = mimeType.name();
    const QString genericIconName = mimeType.genericIconName();

    if (const QString iconName = iconNameForExactMimeType(mimeName); !iconName.isEmpty()) {
        return iconName;
    }

    if (genericIconName == u"x-office-presentation"_s) {
        return u"file-chart"_s;
    }

    if (isDiagramMimeType(mimeName)) {
        return u"file-diagram"_s;
    }

    if (is3dMimeType(mimeName)) {
        return u"file-3d"_s;
    }

    if (mimeName.startsWith(u"image/"_s) || genericIconName == u"image-x-generic"_s) {
        return u"file-image"_s;
    }

    if (mimeName.startsWith(u"audio/"_s) || genericIconName == u"audio-x-generic"_s) {
        return u"file-audio"_s;
    }

    if (mimeName.startsWith(u"video/"_s) || genericIconName == u"video-x-generic"_s) {
        return u"file-video"_s;
    }

    if (mimeName.startsWith(u"font/"_s) || genericIconName == u"font-x-generic"_s) {
        return u"file-font"_s;
    }

    if (genericIconName == u"package-x-generic"_s) {
        return u"file-zip"_s;
    }

    if (genericIconName == u"x-office-spreadsheet"_s) {
        return u"file-grid"_s;
    }

    if (isCodeMimeType(mimeName, genericIconName)) {
        return u"file-code"_s;
    }

    if (genericIconName == u"x-office-document"_s || mimeName.startsWith(u"text/"_s)) {
        return u"file-text"_s;
    }

    return u"file"_s;
}

} // namespace

QString FileIconResolver::iconName(const QString &fileName, const NodeType nodeType) const {
    if (nodeType == NodeType::Directory) {
        // Ignored by the qml view, which renders directories from its own tinted asset.
        return u"folder"_s;
    }

    if (const auto iconIt = _iconNamesByFileName.constFind(fileName); iconIt != _iconNamesByFileName.cend()) {
        return *iconIt;
    }

    QString iconName = iconNameForFileName(fileName);
    if (iconName.isEmpty()) {
        iconName = iconNameForMimeType(QMimeDatabase{}.mimeTypeForFile(fileName, QMimeDatabase::MatchExtension));
    }
    if (_iconNamesByFileName.size() >= maxCacheEntries) {
        _iconNamesByFileName.clear();
    }
    (void) _iconNamesByFileName.insert(fileName, iconName);
    return iconName;
}

void FileIconResolver::clear() const {
    _iconNamesByFileName.clear();
}

} // namespace KDC

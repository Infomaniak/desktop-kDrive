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
#include <QMimeDatabase>
#include <QMimeType>
#include <QSet>

namespace KDC {

namespace {

constexpr qsizetype maxCacheEntries = 1024;

QString iconNameForKnownExtension(const QString &fileName) {
    static const QSet<QString> audioExtensions{QStringLiteral("m3u"), QStringLiteral("m3u8")};
    static const QSet<QString> diagramExtensions{
            QStringLiteral("dot"),
            QStringLiteral("fig"),
            QStringLiteral("graffle"),
            QStringLiteral("jgraph"),
    };
    static const QSet<QString> fontExtensions{QStringLiteral("otf"), QStringLiteral("pcf")};
    static const QSet<QString> codeExtensions{
            QStringLiteral("asm"), QStringLiteral("bash"), QStringLiteral("cjs"),         QStringLiteral("jsx"),
            QStringLiteral("kts"), QStringLiteral("make"), QStringLiteral("pyw"),         QStringLiteral("rs"),
            QStringLiteral("s"),   QStringLiteral("sql"),  QStringLiteral("swift"),       QStringLiteral("ts"),
            QStringLiteral("tsx"), QStringLiteral("zsh"),  QStringLiteral("webmanifest"),
    };
    static const QSet<QString> modelExtensions{QStringLiteral("obj"), QStringLiteral("step"), QStringLiteral("stp")};
    static const QSet<QString> textFileNames{
            QStringLiteral("changelog"),
            QStringLiteral("licence"),
            QStringLiteral("license"),
            QStringLiteral("readme"),
    };

    const QFileInfo fileInfo(fileName);
    const QString extension = fileInfo.suffix().toLower();
    if (audioExtensions.contains(extension)) {
        return QStringLiteral("file-audio");
    }
    if (diagramExtensions.contains(extension)) {
        return QStringLiteral("file-diagram");
    }
    if (fontExtensions.contains(extension)) {
        return QStringLiteral("file-font");
    }
    if (codeExtensions.contains(extension)) {
        return QStringLiteral("file-code");
    }
    if (modelExtensions.contains(extension)) {
        return QStringLiteral("file-3d");
    }
    if (textFileNames.contains(fileInfo.fileName().toLower())) {
        return QStringLiteral("file-text");
    }

    return {};
}

bool isPdfMimeType(const QString &mimeName) {
    return mimeName == QStringLiteral("application/pdf") || mimeName == QStringLiteral("application/x-gzpdf") ||
           mimeName == QStringLiteral("application/x-bzpdf") || mimeName == QStringLiteral("application/x-xzpdf") ||
           mimeName == QStringLiteral("application/x-lzpdf");
}

bool isPresentationMimeType(const QString &mimeName, const QString &genericIconName) {
    return genericIconName == QStringLiteral("x-office-presentation") ||
           mimeName == QStringLiteral("application/vnd.apple.keynote");
}

bool isDiagramMimeType(const QString &mimeName) {
    return mimeName.contains(QStringLiteral("visio")) || mimeName.contains(QStringLiteral("jgraph")) ||
           mimeName.contains(QStringLiteral("omnigraffle")) || mimeName.startsWith(QStringLiteral("application/x-dia")) ||
           mimeName.startsWith(QStringLiteral("application/vnd.oasis.opendocument.graphics")) ||
           mimeName == QStringLiteral("application/vnd.sun.xml.draw") ||
           mimeName == QStringLiteral("application/vnd.sun.xml.draw.template") ||
           mimeName == QStringLiteral("application/vnd.stardivision.draw") || mimeName == QStringLiteral("application/x-kivio") ||
           mimeName == QStringLiteral("text/vnd.graphviz") || mimeName == QStringLiteral("image/x-xfig");
}

bool is3dMimeType(const QString &mimeName) {
    return mimeName.startsWith(QStringLiteral("model/")) || mimeName == QStringLiteral("application/x-blender") ||
           mimeName == QStringLiteral("application/x-kpovmodeler");
}

bool isSpreadsheetMimeType(const QString &mimeName, const QString &genericIconName) {
    return genericIconName == QStringLiteral("x-office-spreadsheet") || mimeName == QStringLiteral("text/csv") ||
           mimeName == QStringLiteral("text/tab-separated-values");
}

bool isArchiveMimeType(const QString &mimeName, const QString &genericIconName) {
    return genericIconName == QStringLiteral("package-x-generic") || mimeName == QStringLiteral("application/vnd.efi.iso") ||
           mimeName == QStringLiteral("application/vnd.appimage") || mimeName == QStringLiteral("application/x-iso9660-appimage");
}

bool isCodeMimeType(const QString &mimeName, const QString &genericIconName) {
    return genericIconName == QStringLiteral("text-x-script") ||
           (mimeName.startsWith(QStringLiteral("text/x-")) && mimeName != QStringLiteral("text/x-log")) ||
           mimeName == QStringLiteral("text/html") || mimeName == QStringLiteral("text/css") ||
           mimeName == QStringLiteral("application/xml") || mimeName == QStringLiteral("application/xml-dtd") ||
           mimeName == QStringLiteral("application/yaml") || mimeName == QStringLiteral("application/toml") ||
           mimeName.endsWith(QStringLiteral("+json")) || mimeName.endsWith(QStringLiteral("+xml")) ||
           mimeName.endsWith(QStringLiteral("+yaml"));
}

QString iconNameForMimeType(const QMimeType &mimeType) {
    if (!mimeType.isValid() || mimeType.isDefault()) {
        return QStringLiteral("file");
    }

    const QString mimeName = mimeType.name();
    const QString genericIconName = mimeType.genericIconName();

    if (mimeName == QStringLiteral("application/vnd.apple.numbers")) {
        return QStringLiteral("file-grid");
    }

    if (isPdfMimeType(mimeName)) {
        return QStringLiteral("file-pdf");
    }

    if (isPresentationMimeType(mimeName, genericIconName)) {
        return QStringLiteral("file-chart");
    }

    if (isDiagramMimeType(mimeName)) {
        return QStringLiteral("file-diagram");
    }

    if (is3dMimeType(mimeName)) {
        return QStringLiteral("file-3d");
    }

    if (mimeName.startsWith(QStringLiteral("image/")) || genericIconName == QStringLiteral("image-x-generic")) {
        return QStringLiteral("file-image");
    }

    if (mimeName.startsWith(QStringLiteral("audio/")) || genericIconName == QStringLiteral("audio-x-generic")) {
        return QStringLiteral("file-audio");
    }

    if (mimeName.startsWith(QStringLiteral("video/")) || genericIconName == QStringLiteral("video-x-generic")) {
        return QStringLiteral("file-video");
    }

    if (mimeName.startsWith(QStringLiteral("font/")) || genericIconName == QStringLiteral("font-x-generic")) {
        return QStringLiteral("file-font");
    }

    if (isArchiveMimeType(mimeName, genericIconName)) {
        return QStringLiteral("file-zip");
    }

    if (isSpreadsheetMimeType(mimeName, genericIconName)) {
        return QStringLiteral("file-grid");
    }

    if (isCodeMimeType(mimeName, genericIconName)) {
        return QStringLiteral("file-code");
    }

    if (genericIconName == QStringLiteral("x-office-document") || mimeName.startsWith(QStringLiteral("text/"))) {
        return QStringLiteral("file-text");
    }

    return QStringLiteral("file");
}

} // namespace

QString FileIconResolver::iconName(const QString &fileName, const NodeType nodeType) const {
    if (nodeType == NodeType::Directory) {
        return QStringLiteral("file");
    }

    if (const auto iconIt = _iconNamesByFileName.constFind(fileName); iconIt != _iconNamesByFileName.cend()) {
        return *iconIt;
    }

    QString iconName = iconNameForKnownExtension(fileName);
    if (iconName.isEmpty()) {
        iconName = iconNameForMimeType(QMimeDatabase{}.mimeTypeForFile(fileName, QMimeDatabase::MatchExtension));
    }
    if (_iconNamesByFileName.size() >= maxCacheEntries) {
        _iconNamesByFileName.clear();
    }
    _iconNamesByFileName.insert(fileName, iconName);
    return iconName;
}

} // namespace KDC

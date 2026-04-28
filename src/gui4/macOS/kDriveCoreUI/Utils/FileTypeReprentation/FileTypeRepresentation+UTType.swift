/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Foundation
import InfomaniakFoundation
import UniformTypeIdentifiers

public extension FileTypeRepresentation {
    init(utType: UTType) {
        if utType.conforms(to: .archive) {
            self = .archive
        } else if utType.conforms(to: .audio) {
            self = .audio
        } else if utType.conforms(to: [.sourceCode, .html, .json, .xml]) {
            self = .code
        } else if utType.conforms(to: [.text, .pages, .onlyOffice, .wordDoc, .wordDocm, .wordDocx]) {
            self = .doc
        } else if utType.conforms(to: [.folder, .directory]) {
            self = .folder
        } else if utType.conforms(to: .font) {
            self = .font
        } else if utType.conforms(to: .spreadsheet) {
            self = .grid
        } else if utType.conforms(to: [.calendarEvent, .ics]) {
            self = .ics
        } else if utType.conforms(to: .image) {
            self = .image
        } else if utType.conforms(to: .pdf) {
            self = .pdf
        } else if utType.conforms(to: .presentation) {
            self = .point
        } else if utType.conforms(to: .vCard) {
            self = .vcard
        } else if utType.conforms(to: [.movie, .video]) {
            self = .video
        } else {
            self = .unknown
        }
    }
}

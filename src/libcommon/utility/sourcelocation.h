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

#ifdef _WIN32
#define SRC_LOC_AVALAIBALE
#endif

#ifdef SRC_LOC_AVALAIBALE
#include <source_location>
#include <filesystem>
#include <string>
#endif // SRC_LOC_AVALAIBALE

namespace KDC {

class SourceLocation {
    public:
        constexpr SourceLocation() = default;

#ifdef SRC_LOC_AVALAIBALE
        [[nodiscard]] static consteval SourceLocation currentLoc(
                const std::source_location& loc = std::source_location::current()) {
            SourceLocation result;
            result._line = loc.line();
            result._fileName = loc.file_name();
            result._functionName = loc.function_name();
            return result;
        }
#else
#define currentLoc() currentLocCompatibility(__LINE__, __FILE__, "unknown") // Cannot be used as a default argument...
        [[nodiscard]] static consteval SourceLocation currentLocCompatibility(uint32_t line, const char* file,
                                                                              const char* function) {
            SourceLocation result;
            result._line = line;
            result._fileName = file;
            result._functionName = function;
            return result;
        }
#endif // SRC_LOC_AVALAIBALE

        [[nodiscard]] uint32_t line() const { return _line; }
        [[nodiscard]] std::string fileName() const { return std::filesystem::path(_fileName).filename().string(); }
        [[nodiscard]] std::string functionName() const {
            std::string str(_functionName);
            auto firstParenthesis = str.find_first_of('(');
            str = firstParenthesis != std::string::npos
                          ? str.substr(0, firstParenthesis)
                          : str; // "namespace::class::function(namespace::args)" -> "namespace::class::function"
            auto lastDot = str.find_last_of(':');
            return lastDot != std::string::npos ? str.substr(lastDot + 1) : str; // "namespace::class::function" -> "function"
        }

    private:
        uint32_t _line = 0;
        const char* _fileName = "";
        const char* _functionName = "";
};

} // namespace KDC
// namespace KDC

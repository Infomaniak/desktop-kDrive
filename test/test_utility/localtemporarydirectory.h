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
#include <string>
#include <filesystem>

namespace KDC {

class AbstractLocalTemporaryDirectory {
    public:
        explicit AbstractLocalTemporaryDirectory(const SyncPath &inputPath = {});
        ~AbstractLocalTemporaryDirectory();

        [[nodiscard]] const std::filesystem::path &path() const { return _path; }
        [[nodiscard]] const NodeId &id() const { return _id; }

    protected:
        SyncPath _inputPath;

        std::filesystem::path _path;
        NodeId _id;

    private:
        virtual void createDirectory() = 0;
};

class LocalTemporaryDirectory : public AbstractLocalTemporaryDirectory {
    public:
        explicit LocalTemporaryDirectory(const std::string &testType = "undef", const SyncPath &destinationPath = {});

    private:
        void createDirectory() override;

        std::string _testType;
};

class LocalTemporaryDirectoryFromAbsolutePath : public AbstractLocalTemporaryDirectory {
    public:
        enum class RecursiveMode {
            NonRecursive,
            Recursive
        };
        explicit LocalTemporaryDirectoryFromAbsolutePath(const SyncPath &absolutePath,
                                                         RecursiveMode recursiveMode = RecursiveMode::NonRecursive);

    private:
        void createDirectory() override;

        RecursiveMode _recursiveMode{RecursiveMode::NonRecursive};
};

} // namespace KDC

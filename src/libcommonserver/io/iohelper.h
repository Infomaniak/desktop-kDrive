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

#include "libcommon/utility/types.h"
#include "libcommonserver/log/log.h"
#ifdef _WIN32
#include <AccCtrl.h>
#endif

namespace KDC {

struct FileStat;

struct IoHelper {
    public:
        IoHelper(){};

        inline static void setLogger(log4cplus::Logger logger) { _logger = logger; }

#ifdef _WIN32
        static int _getAndSetRightsMethod;
#endif
        
        static IoError stdError2ioError(int error) noexcept;
        static IoError stdError2ioError(const std::error_code &ec) noexcept;
        static IoError posixError2ioError(int error) noexcept;
        static std::string ioError2StdString(IoError ioError) noexcept;

        static bool fileExists(const std::error_code &ec) noexcept;
#ifdef __APPLE__
        static IoError nsError2ioError(int nsErrorCode) noexcept;
#endif

        //! Get the item type of the item indicated by `path`.
        /*!
          \param path is the file system path of the inspected item.
          \param itemType is the type of the item indicated by `path`.
          \return true if no unexpected error occurred, false otherwise.
            See _isExpectedError for the definition of an expected error.
        */
        static bool getItemType(const SyncPath &path, ItemType &itemType) noexcept;

        //! Returns the directory location suitable for temporary files.
        /*!
         \param directoryPath is a path to a directory suitable for temporary files. Empty if there is a an error.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool tempDirectoryPath(SyncPath &directoryPath, IoError &ioError) noexcept;

        //! Returns the log directory path of the application.
        /*!
         \param directoryPath is set with the path of to the log directory of the application. Empty if there is a an error.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool logDirectoryPath(SyncPath &directoryPath, IoError &ioError) noexcept;

        //! Retrieves the node identifier of the item indicated by a file system path.
        /*!
         \param path is a file system path to a directory entry (we also call it an item).
         \param nodeId is set with the node identifier of the item indicated by path, if the item exists, otherwise an empty
         string. The node identifier is the nodeid of the item on Linux and Mac systems; the Windows file id otherwise. \return
         true if no unexpected error occurred, false otherwise.
         */
        static bool getNodeId(const SyncPath &path, NodeId &nodeId) noexcept;

        //! Retrieves the file status of the item indicated by a file system path.
        /*!
         \param path is a file system path to a directory entry (we also call it an item).
         \param filestat is set with the file status of the item indicated by path, if the item exists and the status could be
         succesfully retrieved, nullptr otherwise. \param exists is set with true if path indicates an existing item (e.g., a
         dangling symlink is an existing item of file type), false otherwise. \param ioError holds the error returned when an
         underlying OS API call fails. \return true if no unexpected error occurred, false otherwise.
         */
        static bool getFileStat(const SyncPath &path, FileStat *filestat, bool &exists, IoError &ioError) noexcept;

        // The following prototype throws a std::runtime_error if some unexpected error is encountered when trying to retrieve the
        // file status. This is a convenience function to be used in tests only.
        static void getFileStat(const SyncPath &path, FileStat *filestat, bool &exists);

        //! Check if the item indicated by path has a size or a modification date different from the specified ones.
        /*!
         \param path is a file system path to a directory entry (we also call it an item).
         \param previousSize is a file size in bytes to be checked against.
         \param previousMtime is the previous modification date to be checked against.
         \param ioError holds the error returned when an underlying OS API call fails.
         \param changed is a boolean set with true if the check is successful and the file has changed with respect to size or
         modification time. False otherwise. \return true if no unexpected error occurred, false otherwise.
         */
        static bool checkIfFileChanged(const SyncPath &path, int64_t previousSize, time_t previousMtime, bool &changed,
                                       IoError &ioError) noexcept;

        //! Check if the item indicated by path is hidden.
        /*!
         \param path is a file system path to a directory entry (we also call it an item).
         \param isHidden is a boolean set true if the item indicated by path is hidden, or if the check failed.
            Typically, a file or directory is hidden if it is not visible in a file finder or in a terminal using a sheer `ls`
         command. \param ioError holds the error returned when an underlying OS API call fails. \return true if no unexpected
         error occurred, false otherwise.
         */
        static bool checkIfIsHiddenFile(const SyncPath &path, bool checkAncestors, bool &isHidden, IoError &ioError) noexcept;
        static bool checkIfIsHiddenFile(const SyncPath &path, bool &isHidden, IoError &ioError) noexcept;

#if defined(__APPLE__) || defined(WIN32)
        //! Hides or reveals the item indicated by path.
        /*!
         \param path is a file system path to a directory entry (we also call it an item).
         \param hidden is a boolean indicating whether the item should be hidden or visible (e.g. in a file finder or in a
         terminal when using a sheer `ls` command).
         */
        static void setFileHidden(const SyncPath &path, bool hidden) noexcept;
#endif

        //! Checks if the item indicated by the specified path exists.
        /*!
         \param path is the file system path indicating the item to check.
         \param exists is a boolean set with true if an item indicated by the path exists, false otherwise.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool checkIfPathExists(const SyncPath &path, bool &exists, IoError &ioError) noexcept;

        //! Checks if the item indicated by the specified path exists and has the specified node identifier.
        /*!
         \param path is the file system path indicating the item to check.
         \param nodeId is node identifier that is checked against the identifier indicated by path.
         \param exists is a boolean set with true if an item indicated by the path exists with the specified node identifier,
         false otherwise. \param ioError holds the error returned when an underlying OS API call fails. \return true if no
         unexpected error occurred, false otherwise.
         */
        static bool checkIfPathExistsWithSameNodeId(const SyncPath &path, const NodeId &nodeId, bool &exists,
                                                    IoError &ioError) noexcept;

        //! Get the size of the file indicated by `path`, in bytes.
        /*!
          \param path is the file system path of a file.
          \param size holds the size in bytes of the file indicated by path in case of success.
          \param ioError holds the error associated to a failure of the underlying OS API call, if any.
          \return true if no unexpected error occurred, false otherwise. If path indicates a directory,
            the function returns false and ioError is set with IoErrorIsADirectory.
        */
        static bool getFileSize(const SyncPath &path, uint64_t &size, IoError &ioError);

        //! Check if the file indicated by `path` is accessible.
        //! This is especially useful on Windows where the OS will send a CREATE event while the file is still being copied.
        /*!
          \param absolutePath is the file system path of a file.
          \param ioError holds the error associated to a failure of the underlying OS API call, if any.
          \return true if file is accessible, false otherwise.
        */
        static bool isFileAccessible(const SyncPath &absolutePath, IoError &ioError);

        //! Create a symlink located under `path` to the item indicated by `targetPath`.
        /*!
          \param targetPath is the file system path of the item to link. This can be a path to a directory.
          \param path is the file system path of the symlink to create.
          \param ioError holds the error associated to a failure of the underlying OS API call, if any.
          \return true if no unexpected error occurred, false otherwise.
        */
        static bool createSymlink(const SyncPath &targetPath, const SyncPath &path, IoError &ioError) noexcept;

#ifdef __APPLE__
        //! Create a Finder alias file for the specified target under the specified path.
        /*!
         \param targetPath is the file system path of the target item.
         \param aliasPath is the file system location of the alias file to create.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool createAliasFromPath(const SyncPath &targetPath, const SyncPath &aliasPath, IoError &ioError) noexcept;

        static bool createAlias(const std::string &data, const SyncPath &aliasPath, IoError &ioError) noexcept;
        static bool readAlias(const SyncPath &aliasPath, std::string &data, SyncPath &targetPath, IoError &ioError) noexcept;
#endif

        //! Check if the item indicated by `path` is a directory.
        /*!
         \param path is the file system path of the item to check for.
         \param isDirectory is boolean that is set to true if the type of the item indicated by path is `NodeTypeFile`, false
         otherwise. \param ioError holds the error returned when an underlying OS API call fails. Defaults to false. \return true
         if no unexpected error occurred, false otherwise. If the return value is false, isDirectory is also set with false.
         */
        static bool checkIfIsDirectory(const SyncPath &path, bool &isDirectory, IoError &ioError) noexcept;

        //! Create a directory located under the specified path.
        /*!
         \param path is the file system path of the directory to create.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise. If path indicates an existing directory, then the function
                returns false and sets ioError with IoErrorDirectoryExists.
         */
        static bool createDirectory(const SyncPath &path, IoError &ioError) noexcept;

#ifdef __APPLE__
        // From `man xattr`:
        // Extended attributes are arbitrary metadata stored with a file, but separate from the
        // filesystem attributes (such as modification time or file size). The metadata is often a null-terminated UTF-8 string,
        // but can also be arbitrary binary data.
        //! Retrieves the value of the extended attribute with specified name for the item indicated by path.
        /*!
         \param path is the file system path of the item.
         \param attrName is the name of the extended attribute.
         \param value holds the value of the queried attribute. It is set with the empty string if an error occurrs.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool getXAttrValue(const SyncPath &path, const std::string &attrName, std::string &value,
                                  IoError &ioError) noexcept;
        //! Sets the value of the extended attribute with specified name for the item indicated by path.
        /*!
         \param path is the file system path of the item.
         \param attrName is the name of the extended attribute to be set.
         \param value holds the value of the attribute to be set with.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool setXAttrValue(const SyncPath &path, const std::string &attrName, const std::string &value,
                                  IoError &ioError) noexcept;
#endif

#ifdef _WIN32
#ifndef _WINDEF_
        typedef unsigned long DWORD;
#endif
        static bool getXAttrValue(const SyncPath &path, DWORD attrCode, bool &value, IoError &ioError) noexcept;
        static bool setXAttrValue(const SyncPath &path, DWORD attrCode, IoError &ioError) noexcept;

        // TODO: docstring and unit tests
        static bool checkIfIsJunction(const SyncPath &path, bool &isJunction, IoError &ioError) noexcept;
        static bool createJunction(const std::string &data, const SyncPath &path, IoError &ioError) noexcept;
        static bool readJunction(const SyncPath &path, std::string &data, SyncPath &targetPath, IoError &ioError) noexcept;
        static bool createJunctionFromPath(const SyncPath &targetPath, const SyncPath &path, IoError &ioError) noexcept;
#endif
        //! Checks if the item indicated by the specified path is dehydrated.
        /*!
         \param path is the file system path indicating the item to check.
         \param isDehydrated is set with true if the item indicated by path is dehydrated and no error occured, false otherwise.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool checkIfFileIsDehydrated(const SyncPath &path, bool &isDehydrated, IoError &ioError) noexcept;

        //! Get the rights of the item indicated by `path`.
        /*!
         \param path is the file system path of the item.
         \param read is a boolean indicating whether the item is readable.
         \param write is a boolean indicating whether the item is writable.
         \param exec is a boolean indicating whether the item is executable.
         \param exists is a boolean indicating whether the item exists.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool getRights(const SyncPath &path, bool &read, bool &write, bool &exec, IoError &ioError) noexcept;

        //! Set the rights of the item indicated by `path`.
        /*!
         \param path is the file system path of the item.
         \param read is a boolean indicating whether the item should be readable.
         \param write is a boolean indicating whether the item should be writable.
         \param exec is a boolean indicating whether the item should be executable.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
        */
        static bool setRights(const SyncPath &path, bool read, bool write, bool exec, IoError &ioError) noexcept;

    protected:
        // These functions default to the std::filesystem functions.
        // They can be modified in tests.
        static std::function<bool(const SyncPath &path, std::error_code &ec)> _isDirectory;
        static std::function<bool(const SyncPath &path, std::error_code &ec)> _isSymlink;
        static std::function<SyncPath(const SyncPath &path, std::error_code &ec)> _readSymlink;
        static std::function<std::uintmax_t(const SyncPath &path, std::error_code &ec)> _fileSize;
        static std::function<SyncPath(std::error_code &ec)> _tempDirectoryPath;

#ifdef __APPLE__
        // Can be modified in tests.
        static std::function<bool(const SyncPath &path, SyncPath &targetPath, IoError &ioError)> _readAlias;
#endif

    private:
        static log4cplus::Logger _logger;
        inline static log4cplus::Logger logger() { return Log::isSet() ? Log::instance()->getLogger() : _logger; }

        // The most common and expected errors during IO operations
        static bool _isExpectedError(IoError ioError) noexcept;
#ifdef __APPLE__
        static bool _checkIfAlias(const SyncPath &path, bool &isAlias, IoError &ioError) noexcept;
#endif
        static bool _setTargetType(ItemType &itemType) noexcept;
        static bool _checkIfIsHiddenFile(const SyncPath &path, bool &isHidden, IoError &ioError) noexcept;

        static bool _setRightsStd(const SyncPath &path, bool read, bool write, bool exec, IoError &ioError) noexcept;
};

}  // namespace KDC

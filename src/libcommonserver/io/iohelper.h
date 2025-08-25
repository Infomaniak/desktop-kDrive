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

#pragma once

#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"

#if defined(KD_WINDOWS)
#include <AccCtrl.h>
#endif

namespace KDC {

#if defined(KD_MACOS)
namespace litesync_attrs {

//! Item status
static constexpr std::string_view status("com.infomaniak.drive.desktopclient.litesync.status");
//! Request
static constexpr std::string_view pinState("com.infomaniak.drive.desktopclient.litesync.pinstate");

//! The status of a dehydrated item
static constexpr std::string_view statusOnline("O");
//! The status of an hydrated item
static constexpr std::string_view statusOffline("F");
//! The status of a file during hydration is Hxx where xx is the % of progress
static constexpr std::string_view statusHydrating("H");

//! The placeholder’s content must be dehydrated
static constexpr std::string_view pinStateUnpinned("U");
//! The placeholder’s content must be hydrated
static constexpr std::string_view pinStatePinned("P");
//! The placeholder will not be synced
static constexpr std::string_view pinStateExcluded("E");

} // namespace litesync_attrs
#endif

struct FileStat;

struct IoHelper {
    public:
        class DirectoryIterator {
            public:
                DirectoryIterator(const SyncPath &directoryPath, bool recursive, IoError &ioError);

                DirectoryIterator() = default;
                //! Get the next directory entry.
                /*!
                  \param nextEntry is set with the next directory entry.
                  \param ioError holds the error returned when an underlying OS API call fails.
                  \return true if no error occurred, false otherwise.
                */
                bool next(DirectoryEntry &nextEntry, bool &endOfDirectory, IoError &ioError);
                void disableRecursionPending();

            private:
                bool _recursive = false;
                bool _firstElement = true;
                bool _invalid = false;
                SyncPath _directoryPath;
                std::filesystem::recursive_directory_iterator _dirIterator;
        };
        IoHelper() = default;

        inline static void setLogger(const log4cplus::Logger &logger) { _logger = logger; }
        static IoError stdError2ioError(int error) noexcept;
        static IoError stdError2ioError(const std::error_code &ec) noexcept;
        static IoError posixError2ioError(int error) noexcept;
        static std::string ioError2StdString(IoError ioError) noexcept;

        //! Get the item type of the item indicated by `path`.
        /*!
          \param path is the file system path of the inspected item.
          \param itemType is the type of the item indicated by `path`.
          \return true if no unexpected error occurred, false otherwise.
            See isExpectedError for the definition of an expected error.
        */
        [[nodiscard]] static bool getItemType(const SyncPath &path, ItemType &itemType) noexcept;

        //! Returns the directory location suitable for temporary files.
        /*!
         \param directoryPath is a path to a directory suitable for temporary files. Empty if there is an error.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool tempDirectoryPath(SyncPath &directoryPath, IoError &ioError) noexcept;


        //! Returns the directory location suitable for temporary files.
        /*! This directory is deleted at the end of the application run.
          ! The location of this folder can be enforce with the env variable: KDRIVE_CACHE_PATH
         \param directoryPath is a path to a directory suitable for temporary files. Empty if there is an error.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool cacheDirectoryPath(SyncPath &directoryPath) noexcept;

        //! Returns the log directory path of the application.
        /*!
         \param directoryPath is set with the path of to the log directory of the application. Empty if there is an error.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool logDirectoryPath(SyncPath &directoryPath, IoError &ioError) noexcept;

        //! Returns the log archiver directory path of the application.
        /*!
         \param directoryPath is set with the path of to the log directory of the application. Empty if there is an error.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool logArchiverDirectoryPath(SyncPath &directoryPath, IoError &ioError) noexcept;

        //! Retrieves the node identifier of the item indicated by a file system path.
        /*!
         \param path is a file system path to a directory entry (we also call it an item).
         \param nodeId is set with the node identifier of the item indicated by path, if the item exists, otherwise an empty
         string. The node identifier is the nodeid of the item on Linux and Mac systems; the Windows file id otherwise.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool getNodeId(const SyncPath &path, NodeId &nodeId) noexcept;

        //! Retrieves the file status of the item indicated by a file system path.
        /*!
         \param path is a file system path to a directory entry (we also call it an item).
         \param filestat is set with the file status of the item indicated by path, if the item exists and the status could be
         successfully retrieved, nullptr otherwise.
         !!! For a symlink, filestat.nodeType is set with the type of the target !!!
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool getFileStat(const SyncPath &path, FileStat *filestat, IoError &ioError) noexcept;

        // The following prototype throws a std::runtime_error if some unexpected error is encountered when trying to retrieve the
        // file status. This is a convenience function to be used in tests only.
        static void getFileStat(const SyncPath &path, FileStat *filestat, bool &exists);

        //! Check if the item indicated by path has a size or a modification date different from the specified ones.
        /*!
         \param path is a file system path to a directory entry (we also call it an item).
         \param previousSize is a file size in bytes to be checked against.
         \param previousMtime is the previous modification date to be checked against.
         \param previousBirthtime is the previous creation date to be checked against.
         \param ioError holds the error returned when an underlying OS API call fails.
         \param changed is a boolean set with true if the check is successful and the file has changed with respect to size or
         modification time. False otherwise.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool checkIfFileChanged(const SyncPath &path, int64_t previousSize, SyncTime previousMtime,
                                       SyncTime previousCreationTime, bool &changed, IoError &ioError) noexcept;

        //! Check if the item indicated by path is hidden.
        /*!
         \param path is a file system path to a directory entry (we also call it an item).
         \param isHidden is a boolean set true if the item indicated by path is hidden, or if the check failed. Typically, a file
         or directory is hidden if it is not visible in a file finder or in a terminal using a sheer `ls` command.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool checkIfIsHiddenFile(const SyncPath &path, bool checkAncestors, bool &isHidden, IoError &ioError) noexcept;
        static bool checkIfIsHiddenFile(const SyncPath &path, bool &isHidden, IoError &ioError) noexcept;

#if defined(KD_MACOS) || defined(KD_WINDOWS)
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
         false otherwise.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool checkIfPathExistsWithSameNodeId(const SyncPath &path, const NodeId &nodeId, bool &existsWithSameId,
                                                    NodeId &otherNodeId, IoError &ioError) noexcept;

        //! Get the size of the file indicated by `path`, in bytes.
        /*!
          \param path is the file system path of a file.
          \param size holds the size in bytes of the file indicated by path in case of success.
          \param ioError holds the error associated to a failure of the underlying OS API call, if any.
          \return true if no unexpected error occurred, false otherwise. If path indicates a directory, the function returns false
          and ioError is set with IoError::IsADirectory.
        */
        [[nodiscard]] static bool getFileSize(const SyncPath &path, uint64_t &size, IoError &ioError);

        //! Get the size of the directory indicated by `path` expressed in bytes.
        //! This function is recursive.
        /*!
          \param path is the file system path of a directory.
          \param size holds the size in bytes of the directory indicated by path in case of success.
          \param ioError holds the error associated to a failure of the underlying OS API call, if any.
          \param maxDepth is the maximum depth of the recursion. Defaults to 50.
          \return true if no unexpected error occurred, false otherwise. If path indicates a File,
            the function returns false and ioError is set with IoError::IsADirectory.
        */
        static bool getDirectorySize(const SyncPath &path, uint64_t &size, IoError &ioError, unsigned int maxDepth = 50);

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
          \param isFolder is a boolean set with true if the targetPath is a directory
          \param ioError holds the error associated to a failure of the underlying OS API call, if any.
          \return true if no unexpected error occurred, false otherwise.
        */
        static bool createSymlink(const SyncPath &targetPath, const SyncPath &path, bool isFolder, IoError &ioError) noexcept;

#if defined(KD_MACOS)
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
         \param isDirectory is boolean that is set to true if the type of the item indicated by path is `NodeType::File`, false
         otherwise.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise. If the return value is false, isDirectory is also set with
         false.
         */
        static bool checkIfIsDirectory(const SyncPath &path, bool &isDirectory, IoError &ioError) noexcept;

        //! Create a directory located under the specified path.
        /*!
         \param path is the file system path of the directory to create.
         \param recursive is a boolean indicating whether the missing parent directories should be created as well.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise. If path indicates an existing directory, then the function
         returns false and sets ioError with IoError::DirectoryExists.
         */
        static bool createDirectory(const SyncPath &path, bool recursive, IoError &ioError) noexcept;

        /** Move an item located under the specified path.
         *
         * @param sourcePath is the source file system path of the item to move.
         * @param destinationPath is the destination file system path of the item to move.
         * @param ioError
         * @return
         */
        static bool moveItem(const SyncPath &sourcePath, const SyncPath &destinationPath, IoError &ioError) noexcept;

        /** Rename an item located under the specified path.
         *
         * @param sourcePath is the source file system path of the item to rename.
         * @param destinationPath is the destination file system path of the item to rename.
         * @param ioError holds the error returned when an underlying OS API call fails.
         * @return true if no unexpected error occurred, false otherwise.
         */
        static bool renameItem(const SyncPath &sourcePath, const SyncPath &destinationPath, IoError &ioError) noexcept;

        //! Remove an item located under the specified path.
        /*!
         \param path is the file system path of the item to remove.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool deleteItem(const SyncPath &path, IoError &ioError) noexcept;

        //! Create a directory iterator for the specified path. The iterator can be used to iterate over the items in the
        //! directory.
        /*!
         \param path is the file system path of the directory to iterate over.
         \param recursive is a boolean indicating whether the iterator should be recursive or not.
         \param ioError holds the error returned when an underlying OS API call fails.
         \param iterator is the directory iterator that is set with the directory iterator for the specified path.
         \return true if no unexpected error occurred, false otherwise.
        */
        static bool getDirectoryIterator(const SyncPath &path, bool recursive, IoError &ioError,
                                         DirectoryIterator &iterator) noexcept;

        //! Create a directory entry for the specified path.
        /*!
         * \param path is the file system path of the directory entry to create.
         * \param ioError holds the error returned when an underlying OS API call fails.
         * \entry is the directory entry that is set with the directory entry for the specified path.
         * \return true if no unexpected error occurred, false otherwise.
         */
        static bool getDirectoryEntry(const SyncPath &path, IoError &ioError, DirectoryEntry &entry) noexcept;

        //! Copy the item indicated by `sourcePath` to the location indicated by `destinationPath`.
        /*!
          \param sourcePath is the file system path of the item to copy.
          \param destinationPath is the file system path of the location to copy the item to.
          \param ioError holds the error associated to a failure of the underlying OS API call, if any.
          \return true if no unexpected error occurred, false otherwise.
        */
        static bool copyFileOrDirectory(const SyncPath &sourcePath, const SyncPath &destinationPath, IoError &ioError) noexcept;


#if defined(KD_MACOS)
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
        static bool getXAttrValue(const SyncPath &path, const std::string_view &attrName, std::string &value,
                                  IoError &ioError) noexcept;
        //! Sets the value of the extended attribute with specified name for the item indicated by path.
        /*!
         \param path is the file system path of the item.
         \param attrName is the name of the extended attribute to be set.
         \param value holds the value of the attribute to be set with.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool setXAttrValue(const SyncPath &path, const std::string_view &attrName, const std::string_view &value,
                                  IoError &ioError) noexcept;
        //! Remove the extended attributes with specified names for the item indicated by path.
        /*!
         \param path is the file system path of the item.
         \param attrNames is a vector of the names of the extended attributes to remove.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool removeXAttrs(const SyncPath &path, const std::vector<std::string_view> &attrNames, IoError &ioError) noexcept;
        //! Remove the LiteSync extended attributes for the item indicated by path.
        /*!
         \param path is the file system path of the item.
         \param ioError holds the error returned when an underlying OS API call fails.
         \return true if no unexpected error occurred, false otherwise.
         */
        static bool removeLiteSyncXAttrs(const SyncPath &path, IoError &ioError) noexcept;
        //! Build the 'status' extended attribute value.
        /*!
         \param isSyncing true if the file is hydrating.
         \param progress is the % of progress of the hydration.
         \param isHydrated true if the file is hydrated.
         \return the 'status' extended attribute value.
         */
        static std::string statusXAttr(const bool isSyncing, const int16_t progress, const bool isHydrated) {
            return (isSyncing ? std::string(litesync_attrs::statusHydrating) + std::to_string(progress)
                              : (isHydrated ? std::string(litesync_attrs::statusOffline)
                                            : std::string(litesync_attrs::statusOnline)));
        }
#endif

#if defined(KD_WINDOWS)
#ifndef _WINDEF_
        using DWORD = unsigned long;
#endif
        static bool getXAttrValue(const SyncPath &path, DWORD attrCode, bool &value, IoError &ioError) noexcept;
        static bool setXAttrValue(const SyncPath &path, DWORD attrCode, IoError &ioError) noexcept;

        // TODO: docstring and unit tests
        static bool checkIfIsJunction(const SyncPath &path, bool &isJunction, IoError &ioError) noexcept;
        static bool createJunction(const std::string &data, const SyncPath &path, IoError &ioError) noexcept;
        static bool readJunction(const SyncPath &path, std::string &data, SyncPath &targetPath, IoError &ioError) noexcept;
        static bool createJunctionFromPath(const SyncPath &targetPath, const SyncPath &path, IoError &ioError) noexcept;
        static TRUSTEE &getTrustee();
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

        /**
         * @brief Set the dates using native API.
         * - macOS: If creation date > modification date, creation date is set to modification date
         * - Linux: The creation date cannot be set
         * @param filePath The absolute path to the file to be modified.
         * @param creationDate The creation date to be set.
         * @param modificationDate The modification date to be set.
         * @param symlink A boolean value indicating whether the file is a symlink or not.
         * @return IoErrorSuccess if the process succeeds. An appropriate IoError otherwise.
         */
        static IoError setFileDates(const KDC::SyncPath &filePath, SyncTime creationDate, SyncTime modificationDate,
                                    bool symlink) noexcept;

        static inline bool isLink(LinkType linkType) {
            return linkType == LinkType::Symlink || linkType == LinkType::Hardlink ||
                   (linkType == LinkType::FinderAlias && CommonUtility::isMac()) ||
                   (linkType == LinkType::Junction && CommonUtility::isWindows());
        }

        static inline bool isLinkFollowedByDefault(LinkType linkType) {
            return linkType == LinkType::Symlink || linkType == LinkType::Junction;
        }

        // The most common and expected errors during IO operations
        static bool isExpectedError(IoError ioError) noexcept;

        static bool openFile(const SyncPath &path, std::ifstream &file, IoError &ioError, int timeOut = 10 /*in seconds*/);
        static ExitInfo openFile(const SyncPath &path, std::ifstream &file, int timeOut = 10 /*in seconds*/);
#if defined(KD_WINDOWS)
        static bool getLongPathName(const SyncPath &path, SyncPath &longPathName, IoError &ioError);
        static bool getShortPathName(const SyncPath &path, SyncPath &shortPathName, IoError &ioError);
#endif

    protected:
        friend class DirectoryIterator;
        friend class TestIo;

        // These functions default to the std::filesystem functions.
        // They can be modified in tests.
        static std::function<bool(const SyncPath &path, std::error_code &ec)> _isDirectory;
        static std::function<bool(const SyncPath &path, std::error_code &ec)> _isSymlink;
        static std::function<void(const SyncPath &srcPath, const SyncPath &destPath, std::error_code &ec)> _rename;
        static std::function<SyncPath(const SyncPath &path, std::error_code &ec)> _readSymlink;
        static std::function<std::uintmax_t(const SyncPath &path, std::error_code &ec)> _fileSize;
        static std::function<SyncPath(std::error_code &ec)> _tempDirectoryPath;
#if defined(KD_MACOS)
        // Can be modified in tests.
        static std::function<bool(const SyncPath &path, SyncPath &targetPath, IoError &ioError)> _readAlias;
#endif
        static std::function<bool(const SyncPath &path, FileStat *filestat, IoError &ioError)> _getFileStat;
        static bool _getFileStatFn(const SyncPath &path, FileStat *filestat, IoError &ioError) noexcept;
        static bool _unsuportedFSLogged;
        static void setCacheDirectoryPath(const SyncPath &newPath);

    private:
        static log4cplus::Logger _logger;
        inline static log4cplus::Logger logger() { return Log::isSet() ? Log::instance()->getLogger() : _logger; }

#if defined(KD_MACOS)
        static bool _checkIfAlias(const SyncPath &path, bool &isAlias, IoError &ioError) noexcept;
#endif
        static bool _setTargetType(ItemType &itemType) noexcept;
        static bool _checkIfIsHiddenFile(const SyncPath &path, bool &isHidden, IoError &ioError) noexcept;

        static bool _setRightsStd(const SyncPath &path, bool read, bool write, bool exec, IoError &ioError) noexcept;

#if defined(KD_WINDOWS)
        static bool _setRightsWindowsApiInheritance; // For windows tests only
        static int _getAndSetRightsMethod;
        static std::unique_ptr<BYTE[]> _psid;
        static TRUSTEE _trustee;
        static std::mutex _initRightsWindowsApiMutex;
        static void initRightsWindowsApi();
#endif
};

} // namespace KDC

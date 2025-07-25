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

#define UMDF_USING_NTSTATUS
#include <windows.h>

// Definitions for zwQueryDirectoryFile - Begin
#define MAX_PATH_LENGTH_WIN_LONG 32767

using NTSTATUS = LONG;
#define NT_SUCCESS(Status) (((NTSTATUS) (Status)) >= 0)
#define NT_STATUS(x) ((NTSTATUS) {x})
#define STATUS_SUCCESS ((NTSTATUS) 0x00000000L)
#define STATUS_NO_MORE_FILES ((NTSTATUS) 0x80000006L)
#define STATUS_INVALID_INFO_CLASS ((NTSTATUS) 0xC0000003L)

using UNICODE_STRING = struct {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR Buffer;
};
using PUNICODE_STRING = UNICODE_STRING *;

using IO_STATUS_BLOCK = struct {
        union {
                NTSTATUS Status;
                PVOID Pointer;
        } DUMMYUNIONNAME;
        ULONG_PTR Information;
};
using PIO_STATUS_BLOCK = IO_STATUS_BLOCK *;

typedef VOID(NTAPI *PIO_APC_ROUTINE)(PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, ULONG Reserved);

using FILE_INFORMATION_CLASS = enum _FILE_INFORMATION_CLASS {
    FileDirectoryInformation = 1,
    FileFullDirectoryInformation, // 2
    FileBothDirectoryInformation, // 3
    FileBasicInformation, // 4
    FileStandardInformation, // 5
    FileInternalInformation, // 6
    FileEaInformation, // 7
    FileAccessInformation, // 8
    FileNameInformation, // 9
    FileRenameInformation, // 10
    FileLinkInformation, // 11
    FileNamesInformation, // 12
    FileDispositionInformation, // 13
    FilePositionInformation, // 14
    FileFullEaInformation, // 15
    FileModeInformation, // 16
    FileAlignmentInformation, // 17
    FileAllInformation, // 18
    FileAllocationInformation, // 19
    FileEndOfFileInformation, // 20
    FileAlternateNameInformation, // 21
    FileStreamInformation, // 22
    FilePipeInformation, // 23
    FilePipeLocalInformation, // 24
    FilePipeRemoteInformation, // 25
    FileMailslotQueryInformation, // 26
    FileMailslotSetInformation, // 27
    FileCompressionInformation, // 28
    FileObjectIdInformation, // 29
    FileCompletionInformation, // 30
    FileMoveClusterInformation, // 31
    FileQuotaInformation, // 32
    FileReparsePointInformation, // 33
    FileNetworkOpenInformation, // 34
    FileAttributeTagInformation, // 35
    FileTrackingInformation, // 36
    FileIdBothDirectoryInformation, // 37
    FileIdFullDirectoryInformation, // 38
    FileValidDataLengthInformation, // 39
    FileShortNameInformation, // 40
    FileIoCompletionNotificationInformation, // 41
    FileIoStatusBlockRangeInformation, // 42
    FileIoPriorityHintInformation, // 43
    FileSfioReserveInformation, // 44
    FileSfioVolumeInformation, // 45
    FileHardLinkInformation, // 46
    FileProcessIdsUsingFileInformation, // 47
    FileNormalizedNameInformation, // 48
    FileNetworkPhysicalNameInformation, // 49
    FileIdGlobalTxDirectoryInformation, // 50
    FileIsRemoteDeviceInformation, // 51
    FileAttributeCacheInformation, // 52
    FileNumaNodeInformation, // 53
    FileStandardLinkInformation, // 54
    FileRemoteProtocolInformation, // 55
    FileMaximumInformation
};
using PFILE_INFORMATION_CLASS = FILE_INFORMATION_CLASS *;

using FILE_ID_FULL_DIR_INFORMATION = struct _FILE_ID_FULL_DIR_INFORMATION {
        ULONG NextEntryOffset;
        ULONG FileIndex;
        LARGE_INTEGER CreationTime;
        LARGE_INTEGER LastAccessTime;
        LARGE_INTEGER LastWriteTime;
        LARGE_INTEGER ChangeTime;
        LARGE_INTEGER EndOfFile;
        LARGE_INTEGER AllocationSize;
        ULONG FileAttributes;
        ULONG FileNameLength;
        ULONG EaSize;
        LARGE_INTEGER FileId;
        WCHAR FileName[1];
};
using PFILE_ID_FULL_DIR_INFORMATION = FILE_ID_FULL_DIR_INFORMATION *;

using FILE_ID_BOTH_DIR_INFORMATION = struct _FILE_ID_BOTH_DIR_INFORMATION {
        ULONG NextEntryOffset;
        ULONG FileIndex;
        LARGE_INTEGER CreationTime;
        LARGE_INTEGER LastAccessTime;
        LARGE_INTEGER LastWriteTime;
        LARGE_INTEGER ChangeTime;
        LARGE_INTEGER EndOfFile;
        LARGE_INTEGER AllocationSize;
        ULONG FileAttributes;
        ULONG FileNameLength;
        ULONG EaSize;
        CCHAR ShortNameLength;
        WCHAR ShortName[12];
        LARGE_INTEGER FileId;
        WCHAR FileName[1];
};
using PFILE_ID_BOTH_DIR_INFORMATION = FILE_ID_BOTH_DIR_INFORMATION *;

using FILE_ID_GLOBAL_TX_DIR_INFORMATION = struct _FILE_ID_GLOBAL_TX_DIR_INFORMATION {
        ULONG NextEntryOffset;
        ULONG FileIndex;
        LARGE_INTEGER CreationTime;
        LARGE_INTEGER LastAccessTime;
        LARGE_INTEGER LastWriteTime;
        LARGE_INTEGER ChangeTime;
        LARGE_INTEGER EndOfFile;
        LARGE_INTEGER AllocationSize;
        ULONG FileAttributes;
        ULONG FileNameLength;
        LARGE_INTEGER FileId;
        GUID LockingTransactionId;
        ULONG TxInfoFlags;
        WCHAR FileName[1];
};
using PFILE_ID_GLOBAL_TX_DIR_INFORMATION = FILE_ID_GLOBAL_TX_DIR_INFORMATION *;

#define FILE_ID_GLOBAL_TX_DIR_INFO_FLAG_WRITELOCKED 0x00000001
#define FILE_ID_GLOBAL_TX_DIR_INFO_FLAG_VISIBLE_TO_TX 0x00000002
#define FILE_ID_GLOBAL_TX_DIR_INFO_FLAG_VISIBLE_OUTSIDE_TX 0x00000004

using FILE_OBJECTID_INFORMATION = struct _FILE_OBJECTID_INFORMATION {
        LONGLONG FileReference;
        UCHAR ObjectId[16];
        union {
                struct {
                        UCHAR BirthVolumeId[16];
                        UCHAR BirthObjectId[16];
                        UCHAR DomainId[16];
                } DUMMYSTRUCTNAME;
                UCHAR ExtendedInfo[48];
        } DUMMYUNIONNAME;
};
using PFILE_OBJECTID_INFORMATION = FILE_OBJECTID_INFORMATION *;

typedef NTSTATUS(WINAPI *PZW_QUERY_DIRECTORY_FILE)(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
                                                   PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length,
                                                   FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
                                                   PUNICODE_STRING FileName, BOOLEAN RestartScan);
// Definitions for zwQueryDirectoryFile - End

// Definitions for DeviceIoControl - Begin
using REPARSE_DATA_BUFFER = struct _REPARSE_DATA_BUFFER {
        DWORD ReparseTag;
        WORD ReparseDataLength;
        WORD Reserved;
        union {
                struct {
                        WORD SubstituteNameOffset;
                        WORD SubstituteNameLength;
                        WORD PrintNameOffset;
                        WORD PrintNameLength;
                        ULONG Flags;
                        WCHAR PathBuffer[1];
                } SymbolicLinkReparseBuffer;

                struct {
                        WORD SubstituteNameOffset;
                        WORD SubstituteNameLength;
                        WORD PrintNameOffset;
                        WORD PrintNameLength;
                        WCHAR PathBuffer[1];
                } MountPointReparseBuffer;

                struct {
                        BYTE DataBuffer[1];
                } GenericReparseBuffer;
        };
};

#define REPARSE_DATA_BUFFER_HEADER_SIZE FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)
constexpr auto REPARSE_MOUNTPOINT_HEADER_SIZE = 8;
// Definitions for DeviceIoControl - End

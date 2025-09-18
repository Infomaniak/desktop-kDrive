using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.Types;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using System;
using System.Collections.Generic;
using System.Data.Common;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace Infomaniak.kDrive
{
    public static class Utility
    {
        public static NodeType DeduceNodeTypeFromFilePath(string filePath)
        {
            string fileName = System.IO.Path.GetFileName(filePath);
            string extension = System.IO.Path.GetExtension(fileName).ToLower();
            return extension switch
            {
                ".doc" or ".docx" or ".odt" => NodeType.Doc,
                ".pdf" => NodeType.Pdf,
                ".png" or ".jpg" or ".jpeg" or ".gif" or ".bmp" or ".tiff" or ".svg" => NodeType.Image,
                ".mp4" or ".avi" or ".mov" or ".wmv" or ".flv" => NodeType.Video,
                ".mp3" or ".wav" or ".aac" or ".flac" => NodeType.Audio,
                ".xls" or ".xlsx" or ".ods" => NodeType.Grid,
                _ => NodeType.File,
            };
        }

        public static bool OpenFolderSecurely(string folderPath)
        {
            // Validate input
            if (string.IsNullOrWhiteSpace(folderPath))
            {
                Logger.Log(Logger.Level.Warning, "Cannot open the FolderPath which is null or empty.");
                return false;
            }

            // Prevent UNC paths
            if (folderPath.StartsWith(@"\\", StringComparison.OrdinalIgnoreCase))
            {
                Logger.Log(Logger.Level.Warning, $"Access to UNC paths is restricted ({folderPath}).");
                return false;
            }

            string fullPath;
            try
            {
                fullPath = Path.GetFullPath(folderPath);
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Invalid path format provided ({folderPath}): {ex.Message}");
                return false;
            }

            if (!Directory.Exists(fullPath))
            {
                Logger.Log(Logger.Level.Warning, $"The specified folder does not exist: {fullPath}");
                return false;
            }

            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = "explorer.exe",
                Arguments = $"\"{fullPath}\"",
                UseShellExecute = true,
                CreateNoWindow = true
            };

            Process.Start(startInfo);
            return true;
        }

        public static string DefaultSyncPath(string DriveName)
        {
            string userProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            string defaultPath = Path.Combine(userProfile, $"kDrive {DriveName}");

            if (!Utility.CheckSyncPathValidity(defaultPath, out string errorMessage))
            {
                Logger.Log(Logger.Level.Warning, $"Default sync path '{defaultPath}' is not valid: {errorMessage}. Falling back to numbered paths.");
                int number = 1;
                do
                {
                    defaultPath = Path.Combine(userProfile, number > 0 ? $" ({number})" : "");
                    number++;
                } while (!Utility.CheckSyncPathValidity(defaultPath, out errorMessage) && number < 500);

                if (number >= 500)
                {
                    Logger.Log(Logger.Level.Error, "Unable to find a valid default sync path after 500 attempts.");
                    throw new Exception("Unable to find a valid default sync path after 500 attempts.");
                }
                Logger.Log(Logger.Level.Info, $"Using fallback sync path: {defaultPath}");
            }
            return defaultPath;
        }

        public static bool CheckSyncPathValidity(string path, out string errorMessage)
        {
            errorMessage = string.Empty;
            if (string.IsNullOrWhiteSpace(path))
            {
                errorMessage = "The path cannot be empty.";
                return false;
            }
            // Check for invalid characters
            char[] invalidChars = Path.GetInvalidPathChars();
            if (path.IndexOfAny(invalidChars) >= 0)
            {
                errorMessage = "The path contains invalid characters.";
                return false;
            }
            // Check for reserved names (Windows specific)
            string[] reservedNames = new string[]
            {
                "CON", "PRN", "AUX", "NUL",
                "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
            };
            string directoryName = Path.GetFileName(path.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
            if (reservedNames.Contains(directoryName, StringComparer.OrdinalIgnoreCase))
            {
                errorMessage = $"The path contains a reserved name: {directoryName}.";
                return false;
            }

            // Check if the path is a folder (not a file)
            DirectoryInfo dirInfo = new DirectoryInfo(path);
            if (!dirInfo.Exists)
            {
                return true;
            }

            if (dirInfo.Exists && (dirInfo.Attributes & FileAttributes.Directory) == 0)
            {
                errorMessage = "The specified path is not a directory.";
                return false;
            }

            // Check if the directory is writable
            try
            {
                string testFilePath = Path.Combine(path, Path.GetRandomFileName());
                using (FileStream fs = File.Create(testFilePath, 1, FileOptions.DeleteOnClose))
                {
                    // Successfully created a file, so the directory is writable
                }
            }
            catch (UnauthorizedAccessException)
            {
                errorMessage = "The application does not have permission to write to this directory.";
                return false;
            }
            catch (DirectoryNotFoundException)
            {
                // The directory does not exist, which is normal for a new sync folder
            }
            catch (IOException ioEx)
            {
                errorMessage = $"An I/O error occurred while accessing the directory: {ioEx.Message}";
                return false;
            }
            catch (SecurityException)
            {
                errorMessage = "The application does not have the required security permissions for this directory.";
                return false;
            }
            catch (Exception ex)
            {
                errorMessage = $"An unexpected error occurred: {ex.Message}";
                return false;
            }

            // Check if the directory is not a system or hidden folder
            if (dirInfo.Exists && (dirInfo.Attributes.HasFlag(FileAttributes.System) || dirInfo.Attributes.HasFlag(FileAttributes.Hidden)))
            {
                errorMessage = "The specified directory is a system or hidden folder.";
                return false;
            }

            // Check if the directory is a reparse point (e.g., symbolic link or mount point)
            if (dirInfo.Exists && (dirInfo.Attributes & FileAttributes.ReparsePoint) == FileAttributes.ReparsePoint)
            {
                errorMessage = "The specified directory is a reparse point (e.g., symbolic link or mount point), which is not allowed.";
                return false;
            }

            // Check if the directory is empty
            if (dirInfo.Exists && dirInfo.EnumerateFileSystemInfos().Any())
            {
                errorMessage = "The specified directory is not empty.";
                return false;
            }

            // Additional checks can be added here as needed
            return true;
        }

        public static bool SupportOnlineSync(string path)
        {
            // Check if the path is on an NTFS volume
            var root = Path.GetPathRoot(path);
            if (root == null)
                return false;

            if (path == root)
                return false;

            DriveInfo driveInfo = new DriveInfo(root);
            try
            {
                return driveInfo.DriveFormat.Equals("NTFS", StringComparison.OrdinalIgnoreCase);
            }
            catch (IOException)
            {
                Logger.Log(Logger.Level.Warning, $"IO Exception when accessing drive info for path: {path}");
                return false;
            }
            catch (UnauthorizedAccessException)
            {
                Logger.Log(Logger.Level.Warning, $"Unauthorized Access when accessing drive info for path: {path}");
                return false;
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Warning, $"Unexpected error when accessing drive info for path: {path}. Error: {ex.Message}");
                return false;
            }
        }

        public static void SetWindowProperties(Window window, int width, int height, bool resizable)
        {
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(window);
            var windowId = Win32Interop.GetWindowIdFromWindow(hWnd);
            var appWindow = AppWindow.GetFromWindowId(windowId);
            if (appWindow != null && appWindow.Presenter is OverlappedPresenter presenter)
            {
                presenter.IsMaximizable = false;
                presenter.IsMinimizable = true;
                presenter.IsResizable = false;
                appWindow.Resize(new Windows.Graphics.SizeInt32(width, height));
            }
        }
    }
}

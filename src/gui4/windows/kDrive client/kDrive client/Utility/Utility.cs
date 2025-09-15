using Infomaniak.kDrive.Types;
using System;
using System.Collections.Generic;
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
    }
}

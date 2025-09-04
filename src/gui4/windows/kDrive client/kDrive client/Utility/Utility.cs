using KDrive.Types;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace KDrive
{
    internal static class Utility
    {
        internal static NodeType deduceNodeTypeFromFilePath(string filePath)
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

    }
}

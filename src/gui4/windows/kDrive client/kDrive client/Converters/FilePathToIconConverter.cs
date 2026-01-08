using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class FilePathToIconConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            string path;
            if (value is Uri uri)
            {
                path = uri.LocalPath;
            }
            else if (value is string str)
            {
                path = str;
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "FilePathToIconConverter: value is not a string or Uri.");
                throw new ArgumentException("Invalid item type", nameof(value));
            }

            string extension = System.IO.Path.GetExtension(path).ToLowerInvariant();

            string iconResourceKey = extension.ToLowerInvariant() switch
            {
                // 3D files
                ".stl" or ".obj" or ".fbx" or ".dae" or ".3ds" or ".blend" or ".glb" or ".gltf" or ".ply" or ".x3d"
                    => "Infomaniak.DS.Icons.Documents.file3d",

                // Audio files
                ".mp3" or ".wav" or ".flac" or ".aac" or ".ogg" or ".wma" or ".m4a" or ".aiff" or ".mid" or ".midi"
                    => "Infomaniak.DS.Icons.Documents.file-audio",

                // Chart / data files
                ".csv" or ".xls" or ".xlsx" or ".ods" or ".tsv"
                    => "Infomaniak.DS.Icons.Documents.file-chart",

                // Code / dev files
                ".c" or ".cpp" or ".h" or ".hpp" or ".cs" or ".xaml" or ".java" or ".py" or ".js" or ".ts" or ".jsx" or ".tsx" or ".html" or ".css" or ".scss" or ".json" or ".xml" or ".yml" or ".yaml" or ".php" or ".rb" or ".go" or ".rs" or ".swift" or ".kt" or ".sql" or ".sh" or ".bat" or ".ps1"
                    => "Infomaniak.DS.Icons.Documents.file-code",

                // Diagram / design files
                ".vsd" or ".vsdx" or ".drawio" or ".dia" or ".graffle" or ".fig"
                    => "Infomaniak.DS.Icons.Documents.file-diagram",

                // Font files
                ".ttf" or ".otf" or ".woff" or ".woff2" or ".fon"
                    => "Infomaniak.DS.Icons.Documents.file-font",

                // Grid / spreadsheet data
                ".numbers"
                    => "Infomaniak.DS.Icons.Documents.file-grid",

                // Image files
                ".jpg" or ".jpeg" or ".png" or ".gif" or ".bmp" or ".tif" or ".tiff" or ".ico" or ".svg" or ".webp" or ".heic" or ".heif" or ".avif"
                    => "Infomaniak.DS.Icons.Documents.file-image",

                // PDF
                ".pdf"
                    => "Infomaniak.DS.Icons.Documents.file-pdf",

                // Text / document files
                ".txt" or ".md" or ".log" or ".ini" or ".cfg" or ".jsonl"
                    => "Infomaniak.DS.Icons.Documents.file-text",

                // Video files
                ".mp4" or ".mkv" or ".avi" or ".mov" or ".wmv" or ".flv" or ".webm" or ".m4v" or ".mpeg" or ".mpg"
                    => "Infomaniak.DS.Icons.Documents.file-video",

                // Archive / compressed files
                ".zip" or ".rar" or ".7z" or ".tar" or ".gz" or ".bz2" or ".xz" or ".iso" or ".cab" or ".lz"
                    => "Infomaniak.DS.Icons.Documents.file-zip",

                // Office / rich documents
                ".doc" or ".docx" or ".odt" or ".rtf" or ".pages"
                    => "Infomaniak.DS.Icons.Documents.file-text",

                // Presentation files
                ".ppt" or ".pptx" or ".odp" or ".key"
                    => "Infomaniak.DS.Icons.Documents.file-diagram",

                // Default / unknown
                _ => "Infomaniak.DS.Icons.Documents.file"
            };
            if (iconResourceKey.EndsWith("file"))
            {
                Logger.Log(Logger.Level.Warning, $"FilePathToIconConverter: Using default icon for unknown extension '{extension}' in path '{path}'.");
                // This is not an error, just a warning for tracking purposes.
                // The above warning will lead to a Sentry allowing us to add more extensions in the future.
            }

            if (Microsoft.UI.Xaml.Application.Current.Resources[iconResourceKey] is string iconUriStr)
            {
                if (targetType == typeof(string))
                    return iconUriStr;
                else
                    return new Uri(iconUriStr);
            }
            else
            {
                Logger.Log(Logger.Level.Error, $"Resource for file extension {extension} should be {iconResourceKey} but was not found or is not a string.");
                throw new ArgumentException("Icon resource not found", nameof(value));
            }

        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "StringPathToFileNameConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}
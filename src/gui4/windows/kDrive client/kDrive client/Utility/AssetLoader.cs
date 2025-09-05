using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KDrive
{
    internal static class AssetLoader
    {
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Minor Code Smell", "S1075:URIs should not be hardcoded", Justification = "This is the only place where the asset base path is defined.")]
        static string AssetBasePath = "ms-appx:///Assets";

        public enum AssetType
        {
            Image,
            Icon
        }

        private static string getAssetTypePath(AssetType type)
        {
            return type switch
            {
                AssetType.Image => "Images",
                AssetType.Icon => "Icons",
                _ => throw new ArgumentOutOfRangeException(nameof(type), type, null)
            };
        }

        public static string GetAssetPath(AssetType type, string assetName)
        {
                string assetTypePath = getAssetTypePath(type);
            return Path.Combine(AssetBasePath, assetTypePath, assetName);
        }

        public static Uri GetAssetUri(AssetType type, string assetName)
        {
            return new Uri(GetAssetPath(type, assetName));
        }

    }
}

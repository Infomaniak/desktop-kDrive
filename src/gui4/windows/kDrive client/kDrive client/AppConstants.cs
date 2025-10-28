
using System;

namespace Infomaniak.kDrive
{
    interface IAppConstants
    {
        static abstract Uri StorageUrl { get; }
    }

    class ProductionConstants : IAppConstants
    {
        public static Uri StorageUrl { get; } = new Uri("https://download.storage.infomaniak.com/drive/desktopclient/kDrive");
    }
}

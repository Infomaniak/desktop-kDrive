using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class LogUploadManager : UISafeObservableObject
    {
        private int _percentComplete = 0;
        private LogUploadState _status = LogUploadState.None;
        private DateTime _lastSuccessfullUpload = DateTime.MinValue;

        private bool _uploadInProgress = false;

        public int PercentComplete
        {
            get => _percentComplete;
            set => SetPropertyInUIThread(ref _percentComplete, System.Math.Max(20, value));
        }

        public LogUploadState State
        {
            get => _status;
            set
            {
                SetPropertyInUIThread(ref _status, value);
                UploadInProgress = value switch
                {
                    LogUploadState.Archiving => true,
                    LogUploadState.Uploading => true,
                    LogUploadState.CancelRequested => true,
                    _ => false,
                };
                if (value == LogUploadState.Success)
                    LastSuccessfullUpload = DateTime.Now;
            }
        }

        public bool UploadInProgress
        {
            get => _uploadInProgress;
            private set => SetPropertyInUIThread(ref _uploadInProgress, value);
        }

        public DateTime LastSuccessfullUpload
        {
            get => _lastSuccessfullUpload;
            set => SetPropertyInUIThread(ref _lastSuccessfullUpload, value);
        }

        public static async Task StartUpload(bool includeArchivedLogs)
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            await commService.StartLogUpload(includeArchivedLogs, CancellationToken.None);
        }

        public static async Task CancelUpload()
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            await commService.CancelLogUpload(CancellationToken.None);
        }
    }
}

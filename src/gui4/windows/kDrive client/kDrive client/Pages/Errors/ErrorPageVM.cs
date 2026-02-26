using DynamicData;
using DynamicData.Binding;
using ExCSS;
using Infomaniak.kDrive.CustomControls.Errors;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace Infomaniak.kDrive.Pages.Errors
{
    partial class ErrorPageVM : UISafeObservableObject, IDisposable
    {
        private Sync _syncVM;
        private readonly List<IDisposable?> _errorsSubscription = new();

        public Sync SyncVM
        {
            get => _syncVM;
            set => SetPropertyInUIThread(ref _syncVM, value);
        }

        public ReadOnlyObservableCollection<Error> FileErrors { get; private set; }
        public ReadOnlyObservableCollection<Error> SyncDirErrors { get; private set; }
        public ReadOnlyObservableCollection<Error> OtherErrors { get; private set; }
        public Sync? Sync
        {
            get => _syncVM;
            set
            {
                SetPropertyInUIThread(ref _syncVM, value);
                InitErrorLists();
            }
        }

        public ErrorPageVM()
        {
            InitErrorLists();
        }

        private void InitErrorLists()
        {
            foreach (var subscription in _errorsSubscription)
            {
                subscription?.Dispose();
            }
            _errorsSubscription.Clear();

            if (Sync is null)
            {
                FileErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                SyncDirErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                OtherErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                return;
            }

            _errorsSubscription.Add(_syncVM.SyncErrors
                .ToObservableChangeSet()
                .Filter(e => IsInFileErrorsList(e))
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var fileErrors)
                .Subscribe());
            OnPropertyChangingInUIThread(nameof(FileErrors));
            FileErrors = fileErrors;
            OnPropertyChangedInUIThread(nameof(FileErrors));

            _errorsSubscription.Add(_syncVM.SyncErrors
                .ToObservableChangeSet()
                .Filter(e => IsInSyncDirErrorList(e))
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var syncDirErrors)
                .Subscribe());
            OnPropertyChangingInUIThread(nameof(SyncDirErrors));
            SyncDirErrors = syncDirErrors;
            OnPropertyChangedInUIThread(nameof(SyncDirErrors));

            _errorsSubscription.Add(_syncVM.SyncErrors
                .ToObservableChangeSet()
                .Filter(e => IsInOtherErrorList(e))
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var otherErrors)
                .Subscribe());
            OnPropertyChangingInUIThread(nameof(OtherErrors));
            OtherErrors = otherErrors;
            OnPropertyChangedInUIThread(nameof(OtherErrors));
        }

        private static bool IsInFileErrorsList(Error error)
        {
            return error.ErrorLevel == ErrorLevel.Node;
        }

        private static bool IsInSyncDirErrorList(Error error)
        {
            // List of Types of errors to be included in the SyncDirErrors list:
            List<Type> syncDirErrorTypes = new List<Type>
            {
                typeof(CustomControls.Errors.Templates.SyncPal.SystemErrorSyncDirAccessError),
                typeof(CustomControls.Errors.Templates.SyncPal.SystemErrorSyncDirDiskMissing),
                typeof(CustomControls.Errors.Templates.SyncPal.DataErrorSyncDirChanged)
            };

            Type? errorType = ErrorFactory.GetBestControlType(error);
            if(errorType is null) {
                return false;
            }
            return syncDirErrorTypes.Contains(errorType);
        }

        private static bool IsInOtherErrorList(Error error)
        {
            return !IsInFileErrorsList(error) && !IsInSyncDirErrorList(error);
        }

        public void Dispose()
        {
            foreach (var subscription in _errorsSubscription)
            {
                subscription?.Dispose();
            }
        }
    }
}

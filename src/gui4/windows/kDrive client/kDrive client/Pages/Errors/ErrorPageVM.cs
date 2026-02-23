using DynamicData;
using DynamicData.Binding;
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


        public ErrorPageVM(Sync syncVM)
        {
            _syncVM = syncVM;

            _errorsSubscription.Add(_syncVM.SyncErrors
                .ToObservableChangeSet()
                .Filter(e => IsInFileErrorsList(e))
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var fileErrors)
                .Subscribe());

            FileErrors = fileErrors;

            _errorsSubscription.Add(_syncVM.SyncErrors
                .ToObservableChangeSet()
                .Filter(e => IsInSyncDirErrorList(e))
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var syncDirErrors)
                .Subscribe());
            SyncDirErrors = syncDirErrors;

            _errorsSubscription.Add(_syncVM.SyncErrors
                .ToObservableChangeSet()
                .Filter(e => IsInOtherErrorList(e))
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var otherErrors)
                .Subscribe());
            OtherErrors = otherErrors;
        }

        private static bool IsInFileErrorsList(Error error)
        {
            return error.ErrorLevel == ErrorLevel.Node;
        }

        private static bool IsInSyncDirErrorList(Error error)
        {
            return false; //error.ErrorLevel == ErrorLevel.SyncPal;
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

using DynamicData;
using DynamicData.Binding;
using Infomaniak.kDrive.CustomControls.Errors;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;

namespace Infomaniak.kDrive.Pages.Errors
{
    partial class ErrorPageVM : UISafeObservableObject, IDisposable
    {

        private Sync? _sync;
        private const int _maxConflictsForIndividualDisplay = 5;
        private bool _hasManyConflicts;
        private int _conflictsCount;
        private readonly List<IDisposable?> _errorsSubscription = new();

        public ReadOnlyObservableCollection<Error> FileErrors { get; private set; }
        public ReadOnlyObservableCollection<Error> SyncDirErrors { get; private set; }
        public ReadOnlyObservableCollection<Error> OtherErrors { get; private set; }
        public Sync? Sync
        {
            get => _sync;
            set
            {
                SetPropertyInUIThread(ref _sync, value);
                InitErrorLists();
            }
        }

        public bool HasManyConflicts
        {
            get => _hasManyConflicts;
            private set => SetPropertyInUIThread(ref _hasManyConflicts, value);
        }

        public int ConflictsCount
        {
            get => _conflictsCount;
            private set => SetPropertyInUIThread(ref _conflictsCount, value);
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
            ConflictsCount = Sync.SyncErrors.Where(e => IsConflictUserResolvable(e)).Count();
            HasManyConflicts = ConflictsCount > _maxConflictsForIndividualDisplay;

            _errorsSubscription.Add(Sync.SyncErrors
                .ToObservableChangeSet()
                .Filter(e => IsInFileErrorsList(e))
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var fileErrors)
                .Subscribe());
            OnPropertyChangingInUIThread(nameof(FileErrors));
            FileErrors = fileErrors;
            OnPropertyChangedInUIThread(nameof(FileErrors));

            _errorsSubscription.Add(Sync.SyncErrors
                .ToObservableChangeSet()
                .Filter(e => IsInSyncDirErrorList(e))
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var syncDirErrors)
                .Subscribe());
            OnPropertyChangingInUIThread(nameof(SyncDirErrors));
            SyncDirErrors = syncDirErrors;
            OnPropertyChangedInUIThread(nameof(SyncDirErrors));

            _errorsSubscription.Add(Sync.SyncErrors
                .ToObservableChangeSet()
                .Filter(e => IsInOtherErrorList(e))
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var otherErrors)
                .Subscribe());
            OnPropertyChangingInUIThread(nameof(OtherErrors));
            OtherErrors = otherErrors;
            OnPropertyChangedInUIThread(nameof(OtherErrors));
        }

        private bool IsInFileErrorsList(Error error)
        {
            if (error.ErrorLevel != ErrorLevel.Node)
                return false;

            if (HasManyConflicts && IsConflictUserResolvable(error))
                return false;

            return true;
        }

        private static bool IsConflictUserResolvable(Error error)
        {
            return error.ErrorLevel == ErrorLevel.Node && (error.ConflictType == ConflictType.CreateCreate || error.ConflictType == ConflictType.EditEdit);
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
            if (errorType is null)
            {
                return false;
            }
            return syncDirErrorTypes.Contains(errorType);
        }

        private bool IsInOtherErrorList(Error error)
        {
            return !IsInFileErrorsList(error) && !IsInSyncDirErrorList(error) && !IsConflictUserResolvable(error);
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

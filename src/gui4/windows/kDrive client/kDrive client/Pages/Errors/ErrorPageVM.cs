using DynamicData;
using DynamicData.Binding;
using Infomaniak.kDrive.CustomControls.Errors;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Reactive.Subjects;
using static Infomaniak.kDrive.ViewModels.AppModel;

namespace Infomaniak.kDrive.Pages.Errors
{
    partial class ErrorPageVM : UISafeObservableObject, IDisposable
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();

        private Sync? _sync;
        private const int _maxConflictsForIndividualDisplay = 5;
        private bool _hasManyConflicts;
        private bool _hasError;
        private int _conflictsCount;
        private readonly List<IDisposable?> _errorsSubscription = new();

        public ReadOnlyObservableCollection<Error> FileErrors { get; private set; } = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
        public ReadOnlyObservableCollection<Error> SyncDirErrors { get; private set; } = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
        public ReadOnlyObservableCollection<Error> StorageErrors { get; private set; } = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
        public ReadOnlyObservableCollection<Error> OtherErrors { get; private set; } = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());

        private string _conflitFilterText = "";
        // Emits a new filter predicate each time _conflitFilterText changes, causing DynamicData to re-evaluate the ConflictErrors list.
        private readonly BehaviorSubject<Func<Error, bool>> _conflictFilterSubject = new(e => e.IsConflictUserResolvable());
        public ReadOnlyObservableCollection<Error> FilteredConflictErrors { get; private set; } = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());

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

        public bool HasError
        {
            get => _hasError;
            private set => SetPropertyInUIThread(ref _hasError, value);
        }

        public int ConflictsCount
        {
            get => _conflictsCount;
            private set => SetPropertyInUIThread(ref _conflictsCount, value);
        }

        public string ConflictFilterText
        {
            get => _conflitFilterText;
            set => SetConflictFilterText(value);
        }

        public ErrorPageVM()
        {
            _viewModel.SelectedSyncChanged += OnSelectedSyncChanged;
            Sync = _viewModel.SelectedSync;
            InitErrorLists();
        }

        private void OnSelectedSyncChanged(object? sender, SelectedSyncChangedEventArgs e)
        {
            Sync = e.NewValue;
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
                OnPropertyChangingInUIThread(nameof(FileErrors));
                OnPropertyChangingInUIThread(nameof(SyncDirErrors));
                OnPropertyChangingInUIThread(nameof(StorageErrors));
                OnPropertyChangingInUIThread(nameof(OtherErrors));
                OnPropertyChangingInUIThread(nameof(FilteredConflictErrors));
                FileErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                SyncDirErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                StorageErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                OtherErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                FilteredConflictErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                OnPropertyChangedInUIThread(nameof(FileErrors));
                OnPropertyChangedInUIThread(nameof(SyncDirErrors));
                OnPropertyChangedInUIThread(nameof(StorageErrors));
                OnPropertyChangedInUIThread(nameof(OtherErrors));
                OnPropertyChangedInUIThread(nameof(FilteredConflictErrors));
                ConflictsCount = 0;
                HasManyConflicts = false;
                return;
            }
            ConflictsCount = Sync.SyncErrors.Where(e => e.IsConflictUserResolvable()).Count();
            HasManyConflicts = ConflictsCount > _maxConflictsForIndividualDisplay;
            HasError = Sync.SyncErrors.Count > 0;
            // Re-evaluate ConflictsCount and HasManyConflicts each time SyncErrors changes.
            _errorsSubscription.Add(Sync.SyncErrors
                .ToObservableChangeSet()
                .Filter(e => e.IsConflictUserResolvable())
                .QueryWhenChanged(q => q.Count)
                .Subscribe(count =>
                {
                    ConflictsCount = count;
                    HasManyConflicts = count > _maxConflictsForIndividualDisplay;
                }));

            // Re-evaluate HasError each time SyncErrors changes.
            _errorsSubscription.Add(Sync.SyncErrors
                .ToObservableChangeSet()
                .QueryWhenChanged(q => q.Any())
                .Subscribe(any =>
                {
                    HasError = any;
                }));

            // Subscribe to SyncErrors changes for each error category, applying the appropriate filter for each list.
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
                .Filter(e => IsInStorageErrorList(e))
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var storageErrors)
                .Subscribe());
            OnPropertyChangingInUIThread(nameof(StorageErrors));
            StorageErrors = storageErrors;
            OnPropertyChangedInUIThread(nameof(StorageErrors));

            // Reset the filter predicate for the new Sync, then subscribe using the observable overload
            // so the list re-filters automatically when _conflictFilterSubject emits a new predicate.
            _conflictFilterSubject.OnNext(ConflictFilter);
            _errorsSubscription.Add(Sync.SyncErrors
                .ToObservableChangeSet()
                .Filter(_conflictFilterSubject)
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var conflictErrors)
                .Subscribe());
            OnPropertyChangingInUIThread(nameof(FilteredConflictErrors));
            FilteredConflictErrors = conflictErrors;
            OnPropertyChangedInUIThread(nameof(FilteredConflictErrors));

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

        /// <summary>
        /// Updates the filter text and pushes a new predicate to <see cref="_conflictFilterSubject"/>,
        /// which triggers DynamicData to re-evaluate every item in <see cref="FilteredConflictErrors"/>.
        /// </summary>
        public void SetConflictFilterText(string filterText)
        {
            _conflitFilterText = filterText;
            _conflictFilterSubject.OnNext(ConflictFilter);
        }

        // Predicate combining the resolvable check with a case-insensitive path match.
        private Func<Error, bool> ConflictFilter => e =>
            e.IsConflictUserResolvable() &&
            (string.IsNullOrEmpty(_conflitFilterText) || e.Path.Contains(_conflitFilterText, StringComparison.OrdinalIgnoreCase));

        private bool IsInFileErrorsList(Error error)
        {
            if (error.ErrorLevel != ErrorLevel.Node)
                return false;

            if (HasManyConflicts && error.IsConflictUserResolvable())
                return false;

            if(IsInStorageErrorList(error)) return false;

            return true;
        }


        private static bool IsInSyncDirErrorList(Error error)
        {
            // List of Types of errors to be included in the SyncDirErrors list:
            List<Type> syncDirErrorTypes =
            [
                typeof(CustomControls.Errors.Templates.SyncPal.SystemErrorSyncDirAccessError),
                typeof(CustomControls.Errors.Templates.SyncPal.SystemErrorSyncDirDiskMissing),
                typeof(CustomControls.Errors.Templates.SyncPal.DataErrorSyncDirChanged),
                typeof(CustomControls.Errors.Templates.SyncPal.InvalidSyncSyncDirNestingError),
                typeof(CustomControls.Errors.Templates.SyncPal.InvalidSyncSyncDirAccessError),
                typeof(CustomControls.Errors.Templates.SyncPal.SystemErrorUnableToStartVfs)
            ];

            Type? errorType = ErrorFactory.GetBestControlType(error);
            if (errorType is null)
            {
                return false;
            }
            return syncDirErrorTypes.Contains(errorType);
        }

        private static bool IsInStorageErrorList(Error error)
        {
            // List of Types of errors to be included in the StorageErrors list:
            List<Type> storageErrorTypes =
            [
                typeof(CustomControls.Errors.Templates.SyncPal.SystemErrorNotEnoughDiskSpace),
                typeof(CustomControls.Errors.Templates.Node.QuotaExceededError)
            ];

            Type? errorType = ErrorFactory.GetBestControlType(error);
            if (errorType is null)
            {
                return false;
            }
            return storageErrorTypes.Contains(errorType);
        }

        private bool IsInOtherErrorList(Error error)
        {
            return !IsInFileErrorsList(error) && !IsInSyncDirErrorList(error) && !error.IsConflictUserResolvable() && !IsInStorageErrorList(error);
        }

        public void Dispose()
        {
            foreach (var subscription in _errorsSubscription)
            {
                subscription?.Dispose();
            }
            _conflictFilterSubject.Dispose();
            _viewModel.SelectedSyncChanged -= OnSelectedSyncChanged;
        }
    }
}

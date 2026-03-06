using DynamicData;
using DynamicData.Binding;
using Infomaniak.kDrive.CustomControls.Errors;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using static Infomaniak.kDrive.ViewModels.AppModel;

namespace Infomaniak.kDrive.Pages.Errors
{
    partial class ErrorPageVM : UISafeObservableObject, IDisposable
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();

        private Sync? _sync;
        private const int _maxConflictsForIndividualDisplay = 5;
        private bool _hasManyConflicts;
        private int _conflictsCount;
        private readonly List<IDisposable?> _errorsSubscription = new();

        public ReadOnlyObservableCollection<Error> FileErrors { get; private set; } = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
        public ReadOnlyObservableCollection<Error> SyncDirErrors { get; private set; } = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
        public ReadOnlyObservableCollection<Error> OtherErrors { get; private set; } = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());

        private string _conflitFilterText = "";
        public ReadOnlyObservableCollection<Error> ConflictErrors { get; private set; } = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());

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
                OnPropertyChangingInUIThread(nameof(OtherErrors));
                OnPropertyChangingInUIThread(nameof(ConflictErrors));
                FileErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                SyncDirErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                OtherErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                ConflictErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                OnPropertyChangedInUIThread(nameof(FileErrors));
                OnPropertyChangedInUIThread(nameof(SyncDirErrors));
                OnPropertyChangedInUIThread(nameof(OtherErrors));
                OnPropertyChangedInUIThread(nameof(ConflictErrors));
                return;
            }
            ConflictsCount = Sync.SyncErrors.Where(e => e.IsConflictUserResolvable()).Count();
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
                .Filter(e => e.IsConflictUserResolvable())
                .Sort(SortExpressionComparer<Error>.Ascending(e => e.DbId))
                .Bind(out var conflictErrors)
                .Subscribe());
            OnPropertyChangingInUIThread(nameof(ConflictErrors));
            ConflictErrors = conflictErrors;
            OnPropertyChangedInUIThread(nameof(ConflictErrors));

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

            if (HasManyConflicts && error.IsConflictUserResolvable())
                return false;

            return true;
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
            return !IsInFileErrorsList(error) && !IsInSyncDirErrorList(error) && !error.IsConflictUserResolvable();
        }

        public void Dispose()
        {
            foreach (var subscription in _errorsSubscription)
            {
                subscription?.Dispose();
            }
            _viewModel.SelectedSyncChanged -= OnSelectedSyncChanged;
        }
    }
}

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

        public async Task<bool> SolveConflictsQuick(ConflictResolutionStrategy resolutionStrategy)
        {
            if (Sync is null)
            {
                Logger.Log(Logger.Level.Warning, "Attempted to resolve conflicts quickly, but no sync is currently selected.");
            }

            List<DbId> conflictsToResolve = Sync?.SyncErrors.Where(e => IsConflictUserResolvable(e)).Select(e => e.DbId).ToList() ?? new List<DbId>();

            if (conflictsToResolve.Count == 0)
            {
                Logger.Log(Logger.Level.Info, "No user-resolvable conflicts found to resolve.");
                return true;
            }

            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            return await commService.ResolveConflictsQuick(conflictsToResolve, resolutionStrategy, CancellationToken.None);
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
                FileErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                SyncDirErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                OtherErrors = new ReadOnlyObservableCollection<Error>(new ObservableCollection<Error>());
                OnPropertyChangedInUIThread(nameof(FileErrors));
                OnPropertyChangedInUIThread(nameof(SyncDirErrors));
                OnPropertyChangedInUIThread(nameof(OtherErrors));
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
            List<Type> syncDirErrorTypes =
            [
                typeof(CustomControls.Errors.Templates.SyncPal.SystemErrorSyncDirAccessError),
                typeof(CustomControls.Errors.Templates.SyncPal.SystemErrorSyncDirDiskMissing),
                typeof(CustomControls.Errors.Templates.SyncPal.DataErrorSyncDirChanged)
            ];

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
            _viewModel.SelectedSyncChanged -= OnSelectedSyncChanged;
        }
    }
}

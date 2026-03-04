using Infomaniak.kDrive.ServerCommunication.CommStruct;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls.Errors;

public class ConflictDialogVM : UISafeObservableObject
{
    private List<Error> _errors { get; init; }

    private Error? _currentError;

    private NodeConflictInfo? _localNodeVersionInfo;
    private NodeConflictInfo? _remoteNodeVersionInfo;
    private bool _localIsMostRecent;
    private bool _remoteIsMostRecent;
    private CancellationTokenSource? cts;

    public Error? CurrentError
    {
        get => _currentError;
        set
        {
            SetPropertyInUIThread(ref _currentError, value);
            OnPropertyChangedInUIThread(nameof(CurrentErrorIndex));
            OnPropertyChangedInUIThread(nameof(LocalVersionAbsolutePath));
            OnPropertyChangedInUIThread(nameof(RemoteVersionAbsolutePath));
            LoadVersionInfos();
        }
    }

    public NodeConflictInfo? LocalVersionInfo
    {
        get => _localNodeVersionInfo;
        set => SetPropertyInUIThread(ref _localNodeVersionInfo, value);
    }

    public NodeConflictInfo? RemoteVersionInfo
    {
        get => _remoteNodeVersionInfo;
        set => SetPropertyInUIThread(ref _remoteNodeVersionInfo, value);
    }
    public bool LocalIsMostRecent
    {
        get => _localIsMostRecent;
        set => SetPropertyInUIThread(ref _localIsMostRecent, value);
    }
    public bool RemoteIsMostRecent
    {
        get => _remoteIsMostRecent;
        set => SetPropertyInUIThread(ref _remoteIsMostRecent, value);
    }

    public string LocalVersionAbsolutePath => CurrentError is null ? "" : System.IO.Path.Combine(CurrentError.Sync?.LocalPath ?? "", CurrentError.DestinationPath);
    public string RemoteVersionAbsolutePath => CurrentError is null ? "" : System.IO.Path.Combine(CurrentError.Sync?.LocalPath ?? "", CurrentError.Path);

    public bool MultipleConflicts => _errors.Count > 1;

    public int CurrentErrorIndex
    {
        get
        {
            if (CurrentError is null) return 0;
            return _errors.IndexOf(CurrentError) + 1;
        }
    }

    public int TotalErrors => _errors.Count;

    public ConflictDialogVM(List<Error> errors)
    {
        _errors = errors;
        if (errors.Count < 1)
        {
            Logger.Log(Logger.Level.Error, "ConflictDialogVM initialized with an empty list of errors.");
            return;
        }
        CurrentError = _errors[0];
    }

    private void LoadVersionInfos()
    {
        if (CurrentError is null)
        {
            Logger.Log(Logger.Level.Error, "Attempted to load version info but CurrentError is null.");
            return;
        }

        LocalVersionInfo = null;
        RemoteVersionInfo = null;
        RefreshMostRecentFlags();
        if (cts is not null)
        {
            cts.Cancel();
            cts.Dispose();
        }
        cts = new CancellationTokenSource();

        _ = CurrentError.GetConflictNodeVersionInfo(ReplicaSide.Local, cts.Token).ContinueWith(t =>
        {
            if (cts.Token.IsCancellationRequested) return;
            if (t.IsCompletedSuccessfully)
            {
                LocalVersionInfo = t.Result; 
                RefreshMostRecentFlags();
            }
            else
            {
                Logger.Log(Logger.Level.Error, $"Failed to get local node version info for error at path {CurrentError?.Path}: {t.Exception}");
            }
        });

        _ = CurrentError.GetConflictNodeVersionInfo(ReplicaSide.Remote, cts.Token).ContinueWith(t =>
        {
            if (cts.Token.IsCancellationRequested) return;
            if (t.IsCompletedSuccessfully)
            {
                RemoteVersionInfo = t.Result;
                RefreshMostRecentFlags();
            }
            else
            {
                Logger.Log(Logger.Level.Error, $"Failed to get remote node version info for error at path {CurrentError?.Path}: {t.Exception}");
            }
        });
    }

    private void RefreshMostRecentFlags()
    {
        if (LocalVersionInfo is null || RemoteVersionInfo is null)
        {
            LocalIsMostRecent = false;
            RemoteIsMostRecent = false;
            return;
        }

        if (LocalVersionInfo.LastModificationDate > RemoteVersionInfo.LastModificationDate)
        {
            LocalIsMostRecent = true;
            RemoteIsMostRecent = false;
        }
        else if (RemoteVersionInfo.LastModificationDate > LocalVersionInfo.LastModificationDate)
        {
            LocalIsMostRecent = false;
            RemoteIsMostRecent = true;
        }
        else
        {
            // In the unlikely case where both versions have the exact same last modification date, we consider neither to be more recent than the other
            LocalIsMostRecent = false;
            RemoteIsMostRecent = false;
        }
    }
}

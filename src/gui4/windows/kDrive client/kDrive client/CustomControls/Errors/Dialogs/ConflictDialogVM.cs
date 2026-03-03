using Infomaniak.kDrive.ViewModels;
using System.Collections.Generic;

namespace Infomaniak.kDrive.CustomControls.Errors;

public class ConflictDialogVM : UISafeObservableObject
{
    private List<Error> _errors { get; init; }

    private Error? _currentError;

    public Error? CurrentError
    {
        get => _currentError;
        set
        {
            SetPropertyInUIThread(ref _currentError, value);
            OnPropertyChangedInUIThread(nameof(CurrentErrorIndex));
        }
    }

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
}

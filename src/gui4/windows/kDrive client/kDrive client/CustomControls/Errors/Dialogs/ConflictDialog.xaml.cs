using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System.Collections.Generic;

namespace Infomaniak.kDrive.CustomControls.Errors;

public class ConflictDialogVM : UISafeObservableObject
{
    private List<Error> _errors { get; init; }

    private Error? _currentError;

    public Error? CurrentError
    {
        get => _currentError;
        set => SetPropertyInUIThread(ref _currentError, value);
    }

    public ConflictDialogVM(List<Error> errors)
    {
        _errors = errors;
        if (errors.Count < 1)
        {
            Logger.Log(Logger.Level.Error, "ConflictDialogVM initialized with an empty list of errors.");
            return;
        }
    }
}
public partial class ConflictDialog : Page
{
    private List<Error> _errors { get; init; }

    private ContentDialog _dialog;

    private ConflictDialogVM ViewModel { get; set; }

    public ConflictDialog(Error error, ContentDialog dialog)
    {
        _errors = new List<Error> { error };
        ViewModel = new ConflictDialogVM(_errors);
        _dialog = dialog;
        _dialog.IsSecondaryButtonEnabled = false;

        InitializeComponent();
    }

    public ConflictDialog(List<Error> errors, ContentDialog dialog)
    {
        _errors = errors;
        ViewModel = new ConflictDialogVM(_errors);

        if (errors.Count < 1)
        {
            Logger.Log(Logger.Level.Error, "ConflictDialog initialized with an empty list of errors.");
            dialog.Hide();
            Utility.ShowUnexpectedErrorTeachingTip();
            return;
        }

        _dialog = dialog;
        _dialog.IsSecondaryButtonEnabled = false;

        InitializeComponent();
    }

    private void RemoteVersionPresenter_Selected(object sender, System.EventArgs e)
    {
        LocalVersionPresenter.IsSelected = false;
        _dialog.IsSecondaryButtonEnabled = true;
    }

    private void LocalVersionPresenter_Selected(object sender, System.EventArgs e)
    {
        RemoteVersionPresenter.IsSelected = false;
        _dialog.IsSecondaryButtonEnabled = true;
    }

    private void VersionPresenter_Unselected(object sender, System.EventArgs e)
    {
        if (!RemoteVersionPresenter.IsSelected && !LocalVersionPresenter.IsSelected)
        {
            _dialog.IsSecondaryButtonEnabled = false;
        }
        else
        {
            _dialog.IsSecondaryButtonEnabled = true;
        }
    }
}

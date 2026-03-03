using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System.Collections.Generic;
using static System.Windows.Forms.VisualStyles.VisualStyleElement.Window;

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
public partial class ConflictDialog : Page
{
    private List<Error> _errors { get; init; }

    private ContentDialog _dialog;

    private ConflictDialogVM ViewModel { get; set; }

    public ConflictDialog(Error error, ContentDialog dialog)
    {
        _errors = new List<Error> { error };
        _dialog = dialog; 
        Init();
        InitializeComponent();
    }

    public ConflictDialog(List<Error> errors, ContentDialog dialog)
    {
        _errors = errors;
        if (errors.Count < 1)
        {
            Logger.Log(Logger.Level.Error, "ConflictDialog initialized with an empty list of errors.");
            dialog.Hide();
            Utility.ShowUnexpectedErrorTeachingTip();
            return;
        }
        _dialog = dialog;
        Init();
        InitializeComponent();
    }

    private void Init()
    {
        ViewModel = new ConflictDialogVM(_errors);
        _dialog.IsSecondaryButtonEnabled = false;
        _dialog.SecondaryButtonClick += _dialog_SecondaryButtonClick;
        RefreshPrimaryButtonText();
    }

    private void _dialog_SecondaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
    {
        if (!ViewModel.MultipleConflicts || ViewModel.CurrentErrorIndex == _errors.Count)
        {
            return;
        }
        else
        {
            ViewModel.CurrentError = _errors[ViewModel.CurrentErrorIndex];
            RefreshPrimaryButtonText();
            args.Cancel = true;
        }
    }

    private void RefreshPrimaryButtonText()
    {
        if (ViewModel.CurrentErrorIndex == _errors.Count)
            _dialog.SecondaryButtonText = Localizer.Instance.GetString("buttonValidate");
        else
            _dialog.SecondaryButtonText = Localizer.Instance.GetString("buttonValidateAndGoNext");
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

    private void RemoteVersionPresenter_Unselected(object sender, System.EventArgs e)
    {
        if (!LocalVersionPresenter.IsSelected)
            _dialog.IsSecondaryButtonEnabled = false;
    }

    private void LocalVersionPresenter_Unselected(object sender, System.EventArgs e)
    {
        if (!RemoteVersionPresenter.IsSelected)
            _dialog.IsSecondaryButtonEnabled = false;
    }
}

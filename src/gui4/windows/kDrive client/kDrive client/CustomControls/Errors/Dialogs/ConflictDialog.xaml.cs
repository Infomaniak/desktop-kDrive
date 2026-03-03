using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System.Collections.Generic;

namespace Infomaniak.kDrive.CustomControls.Errors;

public partial class ConflictDialog : Page
{
    private List<Error> _errors { get; init; }

    private ContentDialog _dialog;

    private ConflictDialogVM ViewModel { get; set; }

    public ConflictDialog(Error error, ContentDialog dialog)
    {
        _dialog = dialog;
        _errors = new List<Error> { error };
        ViewModel = new ConflictDialogVM(_errors);

        Init();
        InitializeComponent();
    }

    public ConflictDialog(List<Error> errors, ContentDialog dialog)
    {
        _dialog = dialog;
        _errors = errors;
        ViewModel = new ConflictDialogVM(_errors);

        if (errors.Count < 1)
        {
            Logger.Log(Logger.Level.Error, "ConflictDialog initialized with an empty list of errors.");
            dialog.Hide();
            Utility.ShowUnexpectedErrorTeachingTip();
            return;
        }
        Init();
        InitializeComponent();
    }

    private void Init()
    {
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

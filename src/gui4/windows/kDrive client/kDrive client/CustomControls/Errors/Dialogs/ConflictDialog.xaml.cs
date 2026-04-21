using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using static System.Windows.Forms.VisualStyles.VisualStyleElement.Window;

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
        _dialog.IsPrimaryButtonEnabled = false;
        _dialog.PrimaryButtonClick += Dialog_PrimaryButtonClick;
        _dialog.CloseButtonClick += Dialog_CloseButtonClick;
        _dialog.Closed += Dialog_Closed;

        _dialog.CloseButtonText = kDrive.Localizer.Instance.GetString("buttonClose");
        RefreshPrimaryButtonText();
    }

    private void Dialog_Closed(ContentDialog sender, ContentDialogClosedEventArgs args)
    {
        _dialog.PrimaryButtonClick -= Dialog_PrimaryButtonClick;
        _dialog.CloseButtonClick -= Dialog_CloseButtonClick;
        _dialog.Closed -= Dialog_Closed;
    }

    private async void Dialog_CloseButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
    {
        if (ViewModel.CurrentErrorIndex > 1)
        {
            // Save all the conflict resolutions choices the user made so far, even if they didn't click on the "Validate" button for the current conflict.
            if (!await ApplyUserChoices())
            {
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }
    }

    private async void Dialog_PrimaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
    {
        if (RemoteVersionPresenter.IsSelected || LocalVersionPresenter.IsSelected)
            ViewModel.SaveCurrentErrorChoice(LocalVersionPresenter.IsSelected ? ConflictResolutionStrategy.KeepLocal : ConflictResolutionStrategy.KeepRemote);

        if (!ViewModel.HasMultipleConflicts || ViewModel.CurrentErrorIndex == _errors.Count)
        {
            // The user has validated the last conflict, we can now apply all their choices on the server
            if (!await ApplyUserChoices())
            {
                Utility.ShowUnexpectedErrorTeachingTip();
                args.Cancel = true;
            }
        }
        else
        {
            ViewModel.CurrentError = _errors[ViewModel.CurrentErrorIndex];
            RefreshPrimaryButtonText();
            args.Cancel = true;
        }
    }

    private async Task<bool> ApplyUserChoices()
    {
        _dialog.IsEnabled = false;
        var result = !await ViewModel.ApplyUserChoices();
        _dialog.IsEnabled = true;
        return result;
    }

    private void RefreshPrimaryButtonText()
    {
        if (ViewModel.CurrentErrorIndex == _errors.Count)
            _dialog.PrimaryButtonText = Localizer.Instance.GetString("buttonValidate");
        else
            _dialog.PrimaryButtonText = Localizer.Instance.GetString("buttonValidateAndGoNext");
    }

    private void RemoteVersionPresenter_Selected(object sender, System.EventArgs e)
    {
        LocalVersionPresenter.IsSelected = false;
        _dialog.IsPrimaryButtonEnabled = true;
    }

    private void LocalVersionPresenter_Selected(object sender, System.EventArgs e)
    {
        RemoteVersionPresenter.IsSelected = false;
        _dialog.IsPrimaryButtonEnabled = true;
    }

    private void RemoteVersionPresenter_Unselected(object sender, System.EventArgs e)
    {
        if (!LocalVersionPresenter.IsSelected)
            _dialog.IsPrimaryButtonEnabled = false;
    }

    private void LocalVersionPresenter_Unselected(object sender, System.EventArgs e)
    {
        if (!RemoteVersionPresenter.IsSelected)
            _dialog.IsPrimaryButtonEnabled = false;
    }
}

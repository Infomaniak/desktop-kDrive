using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class LicensesHyperlink : StackPanel
    {
        public LicensesHyperlink()
        {
            InitializeComponent();
        }

        public AppVersion? DisplayedVersion
        {
            get => (AppVersion?)GetValue(DisplayedVersionProperty);
            set => SetValue(DisplayedVersionProperty, value);

        }

        public static readonly DependencyProperty DisplayedVersionProperty =
         DependencyProperty.Register(
             nameof(DisplayedVersion),
             typeof(AppVersion),
             typeof(UpdateExpander),
             new PropertyMetadata(null, OnDisplayedVersionChanged));

        private static void OnDisplayedVersionChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is not LicensesHyperlink control)
            {
                Logger.Log(Logger.Level.Warning, "DependencyObject is not of type LicensesHyperlink, this is unexpected.");
                return;
            }
            control.Refresh();
        }

        public void Refresh()
        {
            if (DisplayedVersion is null)
            {
                Logger.Log(Logger.Level.Warning, "DisplayedVersion is null, this is unexpected.");
                return;
            }
            ExpandedTextBox.Text = Localizer.Localizer.GetString("aboutAppVersionCopyright", DisplayedVersion.Tag, DisplayedVersion.BuildVersion, DateTime.Now.Year);
        }
    }
}

using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class DynamicTextBlock : UserControl
    {
        public DynamicTextBlock()
        {
            this.InitializeComponent();
        }

        public static readonly DependencyProperty TextTemplateProperty =
            DependencyProperty.Register(
                nameof(TextTemplate),
                typeof(string),
                typeof(DynamicTextBlock),
                new PropertyMetadata(null, OnArgChanged));

        public static readonly DependencyProperty Arg1Property =
            DependencyProperty.Register(
                nameof(Arg1),
                typeof(object),
                typeof(DynamicTextBlock),
                new PropertyMetadata(null, OnArgChanged));

        public static readonly DependencyProperty Arg2Property =
            DependencyProperty.Register(
                nameof(Arg2),
                typeof(object),
                typeof(DynamicTextBlock),
                new PropertyMetadata(null, OnArgChanged));

        public static readonly DependencyProperty Arg3Property =
            DependencyProperty.Register(
                nameof(Arg3),
                typeof(object),
                typeof(DynamicTextBlock),
                new PropertyMetadata(null, OnArgChanged));

        public static readonly DependencyProperty Arg4Property =
            DependencyProperty.Register(
                nameof(Arg4),
                typeof(object),
                typeof(DynamicTextBlock),
                new PropertyMetadata(null, OnArgChanged));

        public static readonly DependencyProperty Arg5Property =
            DependencyProperty.Register(
                nameof(Arg5),
                typeof(object),
                typeof(DynamicTextBlock),
                new PropertyMetadata(null, OnArgChanged));


        public string TextTemplate
        {
            get => (string)GetValue(TextTemplateProperty);
            set => SetValue(TextTemplateProperty, value);
        }

        public object Arg1
        {
            get => GetValue(Arg1Property);
            set => SetValue(Arg1Property, value);
        }

        public object Arg2
        {
            get => GetValue(Arg2Property);
            set => SetValue(Arg2Property, value);
        }

        public object Arg3
        {
            get => GetValue(Arg3Property);
            set => SetValue(Arg3Property, value);
        }

        public object Arg4
        {
            get => GetValue(Arg4Property);
            set => SetValue(Arg4Property, value);
        }

        public object Arg5
        {
            get => GetValue(Arg5Property);
            set => SetValue(Arg5Property, value);
        }

        private static void OnArgChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is DynamicTextBlock dtb)
                dtb.RefreshText();
        }

        private void RefreshText()
        {
            try
            {
                string formatted = TextTemplate ?? string.Empty;

                object[] args = new[] { Arg1, Arg2, Arg3, Arg4, Arg5 };
                int validArgs = Array.FindLastIndex(args, a => a != null) + 1;

                TextBlock.Text = validArgs > 0
                    ? string.Format(formatted, args[..validArgs])
                    : formatted;
            }
            catch (FormatException ex)
            {
                System.Diagnostics.Debug.WriteLine($"FormatException in DynamicTextBox: {ex.Message}");
            }
        }
    }
}

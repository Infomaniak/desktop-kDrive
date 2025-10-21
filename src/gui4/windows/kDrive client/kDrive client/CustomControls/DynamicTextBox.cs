using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls
{
    public partial class DynamicTextBox : TextBox
    {
        public static readonly DependencyProperty TextTemplateProperty =
            DependencyProperty.Register(
                nameof(TextTemplate),
                typeof(string),
                typeof(DynamicTextBox),
                new PropertyMetadata(null, OnArgChanged));


        public static readonly DependencyProperty Arg1Property =
            DependencyProperty.Register(
                nameof(Arg1),
                typeof(object),
                typeof(DynamicTextBox),
                new PropertyMetadata(null, OnArgChanged));

        public static readonly DependencyProperty Arg2Property =
            DependencyProperty.Register(
                nameof(Arg2),
                typeof(object),
                typeof(DynamicTextBox),
                new PropertyMetadata(null, OnArgChanged));

        public static readonly DependencyProperty Arg3Property =
            DependencyProperty.Register(
                nameof(Arg3),
                typeof(object),
                typeof(DynamicTextBox),
                new PropertyMetadata(null, OnArgChanged));

        public static readonly DependencyProperty Arg4Property =
            DependencyProperty.Register(
                nameof(Arg4),
                typeof(object),
                typeof(DynamicTextBox),
                new PropertyMetadata(null, OnArgChanged));

        public static readonly DependencyProperty Arg5Property =
            DependencyProperty.Register(
                nameof(Arg5),
                typeof(object),
                typeof(DynamicTextBox),
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
            if (d is DynamicTextBox dynamicTextBox)
            {
                dynamicTextBox.RefreshText();
            }
        }

        private void RefreshText()
        {
            try
            {
                if (Arg1 is null)
                {
                    Text = string.Format(TextTemplate ?? string.Empty);
                }
                else if (Arg2 is null)
                {
                    Text = string.Format(TextTemplate ?? string.Empty, Arg1);
                }
                else if (Arg3 is null)
                {
                    Text = string.Format(TextTemplate ?? string.Empty, Arg1, Arg2);
                }
                else if (Arg4 is null)
                {
                    Text = string.Format(TextTemplate ?? string.Empty, Arg1, Arg2, Arg3);
                }
                else if (Arg5 is null)
                {
                    Text = string.Format(TextTemplate ?? string.Empty, Arg1, Arg2, Arg3, Arg4);
                }
                else
                {
                    Text = string.Format(TextTemplate ?? string.Empty, Arg1, Arg2, Arg3, Arg4, Arg5);

                }
            }
            catch (FormatException ex)
            {
                // Log or handle the exception as needed
                System.Diagnostics.Debug.WriteLine($"FormatException in DynamicTextBox: {ex.Message}");
            }
        }
    }
}

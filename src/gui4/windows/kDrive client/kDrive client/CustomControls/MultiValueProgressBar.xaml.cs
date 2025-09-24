using CommunityToolkit.Mvvm.ComponentModel;
using DynamicData;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class MultiValueProgressBar : UserControl
    {

        private static readonly DependencyProperty Color1Property =
            DependencyProperty.Register("Color1", typeof(Brush), typeof(MultiValueProgressBar), new PropertyMetadata(null));

        private static readonly DependencyProperty Value1Property =
            DependencyProperty.Register("Value1", typeof(double?), typeof(MultiValueProgressBar), new PropertyMetadata(0.0));


        private static readonly DependencyProperty Color2Property =
            DependencyProperty.Register("Color2", typeof(Brush), typeof(MultiValueProgressBar), new PropertyMetadata(null));

        private static readonly DependencyProperty Value2Property =
            DependencyProperty.Register("Value2", typeof(double?), typeof(MultiValueProgressBar), new PropertyMetadata(0.0));

        private static readonly DependencyProperty Color3Property =
            DependencyProperty.Register("Color3", typeof(Brush), typeof(MultiValueProgressBar), new PropertyMetadata(null));

        private static readonly DependencyProperty Value3Property =
            DependencyProperty.Register("Value3", typeof(double?), typeof(MultiValueProgressBar), new PropertyMetadata(0.0));

        public Brush Color1
        {
            get { return (Brush)GetValue(Color1Property); }
            set { SetValue(Color1Property, value); }
        }

        public double? Value1
        {
            get { return (double?)GetValue(Value1Property); }
            set { SetValue(Value1Property, value); }
        }

        public Brush Color2
        {
            get { return (Brush)GetValue(Color2Property); }
            set { SetValue(Color2Property, value); }
        }

        public double? Value2
        {
            get { return (double?)GetValue(Value2Property); }
            set { SetValue(Value2Property, value); }
        }

        public Brush Color3
        {
            get { return (Brush)GetValue(Color3Property); }
            set { SetValue(Color3Property, value); }
        }

        public double? Value3
        {
            get { return (double?)GetValue(Value3Property); }
            set { SetValue(Value3Property, value); }
        }

        public MultiValueProgressBar()
        {
            InitializeComponent();
        }
    }
}

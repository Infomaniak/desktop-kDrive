using CommunityToolkit.WinUI;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Markup;
using Microsoft.UI.Xaml.Media;
using System;
using System.Reflection;
using System.Threading.Tasks;
using Windows.UI;
using Windows.UI.ViewManagement;

namespace Infomaniak.kDrive
{
    public sealed class ThemeColorExtension : MarkupExtension
    {
        public string Key { get; set; }

        private static UISettings _uiSettings = new();
        private static event Action ThemeChanged = delegate { };

        static ThemeColorExtension()
        {
            _uiSettings.ColorValuesChanged += (_, __) =>
            {
                ThemeChanged?.Invoke();
            };
        }

        private WeakReference<DependencyObject>? _targetRef;
        private DependencyProperty? _targetProperty;

        protected override object ProvideValue(IXamlServiceProvider serviceProvider)
        {
            if (serviceProvider.GetService(typeof(IProvideValueTarget)) is not IProvideValueTarget pvt)
                return Color.FromArgb(255,255,255,255);

            if (pvt?.TargetObject is not DependencyObject targetObject)
                return Color.FromArgb(255, 255, 255, 255);

            var prop = pvt.TargetProperty;
            if (pvt.TargetProperty is not ProvideValueTargetProperty targetProperty)
                return Color.FromArgb(255, 255, 255, 255);

            _targetRef = new WeakReference<DependencyObject>(targetObject);
            _targetProperty = FindDP(targetObject.GetType(), targetProperty.Name);

            ThemeChanged += OnThemeChanged;
            var res = ResolveColor().Result;
            return res;
        }

        static DependencyProperty? FindDP(Type type, string propertyName)
        {
            var field = type.GetField(
                propertyName + "Property",
                BindingFlags.Public | BindingFlags.Static | BindingFlags.FlattenHierarchy);

            return field?.GetValue(null) as DependencyProperty;
        }
        private async void OnThemeChanged()
        {
            if (_targetRef == null) return;

            if (_targetRef.TryGetTarget(out var target))
            {
                var newValue = await ResolveColor();
                await AppModel.UIThreadDispatcher.EnqueueAsync(() =>
                {
                    target.SetValue(_targetProperty, newValue);
                });
            }
        }

        private async Task<Color> ResolveColor()
        {
            // Resolve on UI thread
            Color color = default;
           await AppModel.UIThreadDispatcher.EnqueueAsync(() =>
            {
                if (_targetRef == null) return;
                if (_targetRef.TryGetTarget(out var target))
                {
                    color = ResolveColorInternal();
                }
            });

            return color;
        }

        private Color ResolveColorInternal()
        {
            if (Application.Current.Resources[Key] is SolidColorBrush brush)
                return brush.Color;

            if (Application.Current.Resources[Key] is Color color)
                return color;

            return default;
        }
    }
}
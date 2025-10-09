using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.WinUI;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Threading;

namespace Infomaniak.kDrive
{
    public class UISafeObservableObject : ObservableObject
    {
        protected bool SetPropertyInUIThread<T>([NotNullIfNotNull(nameof(newValue))] ref T field, T newValue, [CallerMemberName] string? propertyName = null)
        {
            var uiDispatcher = AppModel.UIThreadDispatcher;

            if (uiDispatcher.HasThreadAccess)
            {
                return base.SetProperty(ref field, newValue, propertyName);
            }

            if (EqualityComparer<T>.Default.Equals(field, newValue))
            {
                return false;
            }
            OnPropertyChangedInUIThread(propertyName);
            field = newValue;
            OnPropertyChangingInUIThread(propertyName);
            return true;

        }

        protected void OnPropertyChangedInUIThread([CallerMemberName] string? propertyName = null)
        {
            var uiDispatcher = AppModel.UIThreadDispatcher;
            if (uiDispatcher.HasThreadAccess)
            {
                base.OnPropertyChanged(new PropertyChangedEventArgs(propertyName));
                return;
            }

            var tcs = new TaskCompletionSource();
            uiDispatcher.TryEnqueue(() =>
            {
                base.OnPropertyChanged(new PropertyChangedEventArgs(propertyName));
                tcs.SetResult();
            });
            tcs.Task.GetAwaiter().GetResult();
        }

        protected void OnPropertyChangingInUIThread([CallerMemberName] string? propertyName = null)
        {
            var uiDispatcher = AppModel.UIThreadDispatcher;
            if (uiDispatcher.HasThreadAccess)
            {
                base.OnPropertyChanging(new PropertyChangingEventArgs(propertyName));
                return;
            }

            var tcs = new TaskCompletionSource();
            uiDispatcher.TryEnqueue(() =>
            {
                base.OnPropertyChanging(new PropertyChangingEventArgs(propertyName));
                tcs.SetResult();
            });
            tcs.Task.GetAwaiter().GetResult();
        }
    }
}

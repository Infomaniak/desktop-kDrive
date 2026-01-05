using CommunityToolkit.Mvvm.ComponentModel;
using Infomaniak.kDrive.ViewModels;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

namespace Infomaniak.kDrive
{
    /// <summary>
    /// Provides a thread-safe version of <see cref="ObservableObject"/> that ensures
    /// property change notifications are always raised on the UI thread when using
    /// <see cref="SetPropertyInUIThread"/>.
    ///
    /// This class is particularly useful for view models whose properties are bound
    /// to UI elements but may be updated from background threads. Since UI elements
    /// can only be modified from the UI thread, this class helps prevent cross-thread
    /// access issues and simplifies synchronization logic.
    ///
    /// By using <see cref="SetPropertyInUIThread"/>, you avoid the need to manually
    /// dispatch property change notifications to the UI thread in every background
    /// update scenario.
    ///
    /// For properties that are not related to the UI, the regular
    /// <see cref="ObservableObject.SetProperty"/> method can still be used to avoid
    /// unnecessary thread marshalling overhead.
    /// </summary>

    public partial class UISafeObservableObject : ObservableObject
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
            OnPropertyChangingInUIThread(propertyName);
            field = newValue;
            OnPropertyChangedInUIThread(propertyName);
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

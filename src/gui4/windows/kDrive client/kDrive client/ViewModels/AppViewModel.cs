using CommunityToolkit.Mvvm.ComponentModel;
using kDrive_client.ServerCommunication;
using Microsoft.UI.Dispatching;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using DynamicData;
using DynamicData.Binding;

namespace kDrive_client.DataModel
{
    internal class AppModel : ObservableObject
    {
        /** Indicates if the model has been initialized (i.e. data loaded from the server)
         *  This is only set to true after InitializeAsync() has been called and completed.
         *  It is false by default.
         */
        private bool _isInitialized = false;

        /** The currently selected drive in the UI.
         *  This can be null if no drive is selected.
         */
        private Drive? _selectedDrive = null;

        /** The list of users setup in the application.
         *  This is an observable collection, so the UI can bind to it and be notified of changes.
         */
        private ObservableCollection<User> _users = new();

        /** The list of active drives across all users.
         *  This is a read-only observable collection, so the UI can bind to it and be notified of changes.
         *  It is automatically updated when a drive's IsActive property changes or when drives are added/removed from users.
         */
        public ReadOnlyObservableCollection<Drive> ActiveDrives { get; set; }

        /** The dispatcher queue for the UI thread.
         *  This is used to marshal calls to the UI thread when updating observable item.
         *  It must be set in the mainWindow constructor.
         */
        public static DispatcherQueue UIThreadDispatcher { get; set; } = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();

        public Drive? SelectedDrive
        {
            get => _selectedDrive;
            set => SetProperty(ref _selectedDrive, value);
        }

        public AppModel()
        {
            // Create a read-only observable collection of active drives across all users
            _users.ToObservableChangeSet()
                .AutoRefresh(u => u.Drives.Count)
                .TransformMany(u => u.Drives)
                .AutoRefresh(d => d.IsActive)
                .Filter(d => d.IsActive)
                .Bind(out var activeDrives)
                .Subscribe();
            ActiveDrives = activeDrives;

            // Observe changes to ActiveDrives list and ensure SelectedDrive is valid
            ActiveDrives.ToObservableChangeSet()
                       .Subscribe(_ => EnsureValidSelectedDrive());
        }

        private void EnsureValidSelectedDrive()
        {
            // If ActiveDrives is empty, set SelectedDrive to null
            if (ActiveDrives.Count == 0)
            {
                SelectedDrive = null;
                return;
            }

            // If SelectedDrive is null or not in ActiveDrives, pick the first one
            if (_selectedDrive == null || !_selectedDrive.IsActive)
            {
                UIThreadDispatcher.TryEnqueue(() => SelectedDrive = ActiveDrives[0]);
            }
        }

        public ObservableCollection<User> Users
        {
            get => _users;
            set => SetProperty(ref _users, value);
        }

        public bool IsInitialized
        {
            get => _isInitialized;
            set => SetProperty(ref _isInitialized, value);
        }

        /** Initialize the model by loading data from the server.
         *  This method is asynchronous and should be awaited.
         *  It loads the list of users and their properties/drives from the server.
         */
        public async Task InitializeAsync()
        {
            var userDbIds = await ServerCommunication.CommRequests.GetUserDbIds().ConfigureAwait(false);
            if (userDbIds != null)
            {
                List<Task> reloadTasks = new List<Task>();
                List<User> users = new List<User>();
                for (int i = 0; i < userDbIds.Count; i++)
                {
                    User user = new User(userDbIds[i]);
                    reloadTasks.Add(user.Reload());
                    users.Add(user);
                }
                await Task.WhenAll(reloadTasks).ConfigureAwait(false);
                UIThreadDispatcher.TryEnqueue(() =>
                {
                    Users.AddRange(users);
                });
                await Task.Delay(5000).ConfigureAwait(false); // Fake delay to simulate loading time

            }
            UIThreadDispatcher.TryEnqueue(() =>
            {
                IsInitialized = true;
            });
        }

    }
}

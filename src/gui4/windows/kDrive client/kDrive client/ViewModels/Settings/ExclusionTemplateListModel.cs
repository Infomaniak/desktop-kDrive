using DynamicData;
using DynamicData.Binding;
using DynamicData.Kernel;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public partial class ExclusionTemplateListModel : UISafeObservableObject, IDisposable
    {
        // Collection of all the ExclusionTemplate objects
        private ObservableCollection<ExclusionTemplate> _templates = [];
        private readonly ReadOnlyObservableCollection<ExclusionTemplate> _defaultTemplates;
        private readonly ReadOnlyObservableCollection<ExclusionTemplate> _userDefinedTemplates;
        private int _selectedCount;
        private bool _hasSelectedTemplates;
        private CancellationTokenSource? _saveDebounceCts;
        private readonly object _saveDebounceLock = new();

        // Subscription handlers
        private readonly IDisposable _defaultTemplatesSubscription;
        private readonly IDisposable _userDefinedTemplatesSubscription;
        public ExclusionTemplateListModel()
        {
            _defaultTemplatesSubscription = _templates.ToObservableChangeSet().Filter(t => t.Default).Bind(out _defaultTemplates).Subscribe();
            _userDefinedTemplatesSubscription = _templates.ToObservableChangeSet().Filter(t => !t.Default).Bind(out _userDefinedTemplates).Subscribe();
        }

        public void Dispose()
        {
            _defaultTemplatesSubscription.Dispose();
            _userDefinedTemplatesSubscription.Dispose();
            bool hasPendingSave;

            lock (_saveDebounceLock)
            {
                hasPendingSave = _saveDebounceCts is not null;
                _saveDebounceCts?.Cancel();
                _saveDebounceCts?.Dispose();
                _saveDebounceCts = null;
            }
            if (hasPendingSave)
                SaveUserTemplatesImmediateAsync().ContinueWith(task =>
                {
                    if (task.Exception is not null)
                    {
                        Logger.Log(Logger.Level.Error, $"Failed to save exclusion templates on dispose: {task.Exception}");
                    }
                    else if (!task.Result)
                    {
                        Logger.Log(Logger.Level.Error, "Failed to save exclusion templates on dispose: server returned failure.");
                    }
                }, TaskScheduler.Default);
        }

        // Public property to access the collection
        public ObservableCollection<ExclusionTemplate> Templates
        {
            get => _templates;
            set => SetPropertyInUIThread(ref _templates, value);
        }

        public ReadOnlyObservableCollection<ExclusionTemplate> DefaultTemplates => _defaultTemplates;
        public ReadOnlyObservableCollection<ExclusionTemplate> UserDefinedTemplates => _userDefinedTemplates;

        public int SelectedCount
        {
            get => _selectedCount;
            set => SetPropertyInUIThread(ref _selectedCount, value);
        }

        public bool HasSelectedTemplates
        {
            get => _hasSelectedTemplates;
            set => SetPropertyInUIThread(ref _hasSelectedTemplates, value);
        }

        // Method to load/add/remove templates
        public async Task<bool> LoadTemplates()
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            var res = await commService.GetExclusionTemplates(CancellationToken.None);
            if (res is null)
            {
                Logger.Log(Logger.Level.Error, "Failed to load exclusion templates from server.");
                return false;
            }
            _templates.Clear();
            _templates.AddRange(res);
            UpdateSelectedCount();
            return true;
        }

        public async Task AddTemplate(ExclusionTemplate template)
        {
            _templates.Add(template);
            UpdateSelectedCount();
            await SaveUserTemplates();
        }

        public async Task RemoveTemplate(ExclusionTemplate template)
        {
            _templates.Remove(template);
            UpdateSelectedCount();
            await SaveUserTemplates();
        }

        public async Task RemoveTemplates(IEnumerable<ExclusionTemplate> templates)
        {
            _templates.RemoveMany(templates);
            UpdateSelectedCount();
            await SaveUserTemplates();
        }

        public void UpdateSelectedCount()
        {
            SelectedCount = UserDefinedTemplates.Count(t => t.IsSelected);
            HasSelectedTemplates = SelectedCount > 0;
        }

        public async Task SaveUserTemplates()
        {
            CancellationToken token;
            lock (_saveDebounceLock)
            {
                _saveDebounceCts?.Cancel();
                _saveDebounceCts?.Dispose();
                _saveDebounceCts = new CancellationTokenSource();
                token = _saveDebounceCts.Token;
            }

            try
            {
                await Task.Delay(TimeSpan.FromSeconds(10), token);
            }
            catch (TaskCanceledException)
            {
                return;
            }

            lock (_saveDebounceLock)
            {
                _saveDebounceCts?.Dispose();
                _saveDebounceCts = null;
            }
            if (!await SaveUserTemplatesImmediateAsync())
            {
                Logger.Log(Logger.Level.Error, "Failed to save exclusion templates.");
                Templates.Clear();
                await LoadTemplates();
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }

        private async Task<bool> SaveUserTemplatesImmediateAsync()
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            return await commService.SetUserExclusionTemplates(UserDefinedTemplates.AsList(), CancellationToken.None);
        }
    }
}

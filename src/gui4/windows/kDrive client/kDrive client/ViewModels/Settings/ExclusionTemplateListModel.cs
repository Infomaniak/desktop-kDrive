using DynamicData;
using DynamicData.Binding;
using DynamicData.Kernel;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public partial class ExclusionTemplateListModel : UISafeObservableObject, IDisposable
    {
        // Collection of all the ExclusionTemplate objects
        private ObservableCollection<ExclusionTemplate> _templates = new();
        private ReadOnlyObservableCollection<ExclusionTemplate> _defaultTemplates;
        private ReadOnlyObservableCollection<ExclusionTemplate> _userDefinedTemplates;
        private int _selectedCount;
        private bool _hasSelectedTemplates;

        // Subscription handlers
        private IDisposable _defaultTemplatesSubscription;
        private IDisposable _userDefinedTemplatesSubscription;
        public ExclusionTemplateListModel()
        {
            _defaultTemplatesSubscription = _templates.ToObservableChangeSet().Filter(t => t.Default).Bind(out _defaultTemplates).Subscribe();
            _userDefinedTemplatesSubscription = _templates.ToObservableChangeSet().Filter(t => !t.Default).Bind(out _userDefinedTemplates).Subscribe();
        }

        public void Dispose()
        {
            _defaultTemplatesSubscription.Dispose();
            _userDefinedTemplatesSubscription.Dispose();
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
        public async Task LoadTemplates()
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            var res = await commService.GetExclusionTemplates(CancellationToken.None);
            if (res is not null)
            {
                _templates.Clear();
                _templates.AddRange(res);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Failed to load exclusion templates from server.");
            }
        }

        public async Task AddTemplate(ExclusionTemplate template)
        {
            _templates.Add(template);
            await SaveUserTemplates();
        }

        public async Task RemoveTemplate(ExclusionTemplate template)
        {
            _templates.Remove(template);
            await SaveUserTemplates();
        }

        public async Task RemoveTemplates(IEnumerable<ExclusionTemplate> templates)
        {
            _templates.RemoveMany(templates);
            await SaveUserTemplates();
        }

        public void UpdateSelectedCount(int count)
        {
            SelectedCount = count;
            HasSelectedTemplates = count > 0;
        }

        public async Task SaveUserTemplates()
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            await commService.SetUserExclusionTemplates(UserDefinedTemplates.AsList(), CancellationToken.None);
        }
    }
}

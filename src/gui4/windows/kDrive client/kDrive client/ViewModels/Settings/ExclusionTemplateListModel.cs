using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using DynamicData.Binding;
using DynamicData;
using Microsoft.Extensions.DependencyInjection;
using Infomaniak.kDrive.ServerCommunication.Interfaces;

namespace Infomaniak.kDrive.ViewModels
{
    public class ExclusionTemplateListModel : UISafeObservableObject, IDisposable
    {
        // Collection of all the ExclusionTemplate objects
        private ObservableCollection<ExclusionTemplate> _templates = new();
        private ReadOnlyObservableCollection<ExclusionTemplate> _defaultTemplates;
        private ReadOnlyObservableCollection<ExclusionTemplate> _userDefinedTemplates;

        // Subscription handlers
        private IDisposable _defaultTemplatesSubscription;
        private IDisposable _userDefinedTemplatesSubscription;
        public ExclusionTemplateListModel()
        {
            _defaultTemplatesSubscription = _templates.ToObservableChangeSet().Filter(t => t.IsDefault).Bind(out _defaultTemplates).Subscribe();
            _userDefinedTemplatesSubscription = _templates.ToObservableChangeSet().Filter(t => !t.IsDefault).Bind(out _userDefinedTemplates).Subscribe();
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


        // Method to add/remove templates
        public void AddTemplate(ExclusionTemplate template)
        {
            _templates.Add(template);
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            // TODO: Notify server about the new template if needed
        }
        public void RemoveTemplate(ExclusionTemplate template)
        {
            _templates.Remove(template);
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            // TODO: Notify server about the removed template if needed
        }
    }
}

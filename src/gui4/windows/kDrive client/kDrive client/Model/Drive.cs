using CommunityToolkit.Mvvm.Collections;
using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace kDrive_client.Model
{
    internal class Drive : ObservableObject
    {
        private string _name = "";
        private long _id = -1;
        private Color _color = Color.Blue;
        private long _size = 0;
        private long _usedSize = 0;
        private bool _isActive = true; // Indicates if the user choosed to sync this drive
        private List<Sync?> _syncs = new List<Sync?>();

        public string Name
        {
            get => _name;
            set => SetProperty(ref _name, value);
        }

        public long Id
        {
            get => _id;
            set => SetProperty(ref _id, value);
        }

        public Color Color
        {
            get => _color;
            set => SetProperty(ref _color, value);
        }
        public long Size
        {
            get => _size;
            set => SetProperty(ref _size, value);
        }

        public long UsedSize
        {
            get => _usedSize;
            set => SetProperty(ref _usedSize, value);
        }

        public bool IsActive
        {
            get => _isActive;
            set => SetProperty(ref _isActive, value);
        }
        public List<Sync?> Syncs
        {
            get { _syncs = _syncs.Where(d => d != null).ToList(); return _syncs; }
            set => SetProperty(ref _syncs, value);
        }

    }
}
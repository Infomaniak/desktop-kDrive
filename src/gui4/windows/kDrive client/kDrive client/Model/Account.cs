using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace kDrive_client.Model
{
    internal class Account : ObservableObject
    {
        private long _id = -1;
        private List<Drive?> _drives = new List<Drive?>();
        public long Id
        {
            get => _id;
            set => SetProperty(ref _id, value);
        }
        public List<Drive?> Drives
        {
            get { _drives = _drives.Where(d => d != null).ToList(); return _drives; }
            set => SetProperty(ref _drives, value);
        }

        public List<Drive?> ActivatedDrives()
        {
            return Drives.Where(d => (d != null && d.IsActive)).ToList();
        }
    }
}

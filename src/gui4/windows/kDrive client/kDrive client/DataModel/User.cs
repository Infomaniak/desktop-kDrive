using CommunityToolkit.Mvvm.ComponentModel;
using kDrive_client.ServerCommunication;
using Microsoft.UI.Xaml.Media.Imaging;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace kDrive_client.DataModel
{
    internal class User : ObservableObject
    {
        private int _dbId = -1;
        private int _id = -1;
        private string _name = "";
        private string _email = "";
        private Image? _avatar;
        private bool _isConnected = false;
        private bool _isStaff = false;
        private List<Drive?> _drives = new List<Drive?>();

        public User(int dbId)
        {
            DbId = dbId;
        }

        public static User ReadFrom(Utility.BinaryReader reader)
        {
            int dbId = reader.ReadInt32();

            var user = new User(dbId);
            user.Id = reader.ReadInt32();
            user.Name = reader.ReadString();
            user.Email = reader.ReadString();
            user.Avatar = reader.ReadPNG();
            user.IsConnected = reader.ReadBoolean();
            user.IsStaff = reader.ReadBoolean();
            return user;
        }

        // Force a reload of all properties from the server
        public async Task Reload()
        {
            Task[] tasks = new Task[]
            {
               CommRequests.GetUserId(DbId).ContinueWith(t => { if (t.Result != null) Id = t.Result.Value; }),
               CommRequests.GetUserName(DbId).ContinueWith(t => { if (t.Result != null) Name = t.Result; }),
               CommRequests.GetUserEmail(DbId).ContinueWith(t => { if (t.Result != null) Email = t.Result; }),
               CommRequests.GetUserAvatar(DbId).ContinueWith(t => { if (t.Result != null) Avatar = t.Result; }),
               CommRequests.GetUserIsConnected(DbId).ContinueWith(t => { if (t.Result != null) IsConnected = t.Result.Value; }),
               CommRequests.GetUserIsStaff(DbId).ContinueWith(t => { if (t.Result != null) IsStaff = t.Result.Value; }),
                CommRequests.GetUserDrivesDbIds(DbId).ContinueWith(async t =>
                {
                     if (t.Result != null)
                     {
                          List<Drive?> drives = new List<Drive?>();
                          List<Task> driveTasks = new List<Task>();
                          foreach (var driveDbId in t.Result)
                          {
                            Drive? drive = new Drive(driveDbId);
                            driveTasks.Add(drive.Reload());
                            drives.Add(drive);
                          }
                          await Task.WhenAll(driveTasks).ConfigureAwait(false);
                          Drives = drives;
                     }
                }).Unwrap()
            };
            await Task.WhenAll(tasks).ConfigureAwait(false);
        }

        public int DbId
        {
            get => _dbId;
            set => SetProperty(ref _dbId, value);
        }

        public int Id
        {
            get => _id;
            set => SetProperty(ref _id, value);
        }

        public string Name
        {
            get => _name;
            set => SetProperty(ref _name, value);
        }

        public string Email
        {
            get => _email;
            set => SetProperty(ref _email, value);
        }

        public Image? Avatar
        {
            get => _avatar;
            set => SetProperty(ref _avatar, value);
        }

        public bool IsConnected
        {
            get => _isConnected;
            set => SetProperty(ref _isConnected, value);
        }

        public bool IsStaff
        {
            get => _isStaff;
            set => SetProperty(ref _isStaff, value);
        }

        public List<Drive?> Drives
        {
            get { _drives = _drives.Where(a => a != null).ToList(); return _drives; }
            set => SetProperty(ref _drives, value);
        }
    }
}

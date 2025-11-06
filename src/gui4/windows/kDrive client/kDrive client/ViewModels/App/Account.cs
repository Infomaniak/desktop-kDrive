/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using System.Collections.ObjectModel;

namespace Infomaniak.kDrive.ViewModels
{
    public class Account : UISafeObservableObject
    {
        private string _name;
        private DbId _dbId = -1;
        private ObservableCollection<Drive> _drives = new ObservableCollection<Drive>();
        private User _user;
        public Account(DbId dbId, User user)
        {
            DbId = dbId;
            _user = user;
        }

        public DbId DbId
        {
            get => _dbId;
            set => SetPropertyInUIThread(ref _dbId, value);
        }

        public User User
        {
            get => _user;
            set => SetPropertyInUIThread(ref _user, value);
        }
        public ObservableCollection<Drive> Drives
        {
            get => _drives;
            set => SetPropertyInUIThread(ref _drives, value);
        }

        public string Name
        {
            get => string.IsNullOrEmpty(_name) ? $"Account {_dbId}" : _name;
            set => SetPropertyInUIThread(ref _name, value);
        }
    }
}
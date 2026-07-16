/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    /* Exposes async getters/setters for application state values stored on the server. */
    public class AppStateModel
    {
        private readonly IServerCommService _serverCommService;
        private bool? _showV4OnboardingCachedValue = null;

        public AppStateModel(IServerCommService serverCommService)
        {
            _serverCommService = serverCommService;
        }

        /* Returns the value of the ShowV4Onboarding app state, or null on failure. */
        public async Task<bool?> GetShowV4Onboarding(CancellationToken cancellationToken = default)
        {
            if (_showV4OnboardingCachedValue is not null) return _showV4OnboardingCachedValue;

            string? value = await _serverCommService.GetAppState(AppStateKey.ShowV4Onboarding, cancellationToken);
            if (value is null)
            {
                Logger.Log(Logger.Level.Warning, "Failed to get ShowV4Onboarding state from the server.");
                return null;
            }
            _showV4OnboardingCachedValue = value == "1";
            return _showV4OnboardingCachedValue;
        }

        /* Sets the ShowV4Onboarding app state. Returns true on success, false on failure. */
        public async Task<bool> SetShowV4Onboarding(bool showV4Onboarding, CancellationToken cancellationToken = default)
        {
            if (_showV4OnboardingCachedValue is not null)
                _showV4OnboardingCachedValue = showV4Onboarding;

            if (await _serverCommService.SetAppState(AppStateKey.ShowV4Onboarding, showV4Onboarding ? "1" : "0", cancellationToken))
                return true;

            Logger.Log(Logger.Level.Warning, "Failed to set ShowV4Onboarding state on the server.");
            return false;
        }
    }
}

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

using Infomaniak.kDrive.CustomControls;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Collections.Generic;

namespace Infomaniak.kDrive.ViewModels
{
    /// Application-lifetime presenter of the file opening pop-up windows: it subscribes once to the
    /// <see cref="FileOpeningController"/> events and shows a standalone window per active request,
    /// independently of the main window. The client may run tray-only, or be started cold by a deeplink,
    /// in which cases the main window does not exist and the requests would otherwise be reported
    /// without any progress or cancellation interface.
    public class FileOpeningWindowPresenter
    {
        private readonly FileOpeningController _controller = App.ServiceProvider.GetRequiredService<AppModel>().FileOpeningController;
        private readonly Dictionary<ulong, FileOpeningWindow> _windowsByJobId = new();

        /// Subscribes to the controller and replays the requests it already tracks, so that pop-up windows
        /// appear for the requests reported by the server before this presenter was started. Must be called
        /// on the UI thread, before the server communication starts delivering signals.
        public void Start()
        {
            _controller.ShowRequested += OnShowRequested;
            _controller.DismissRequested += OnDismissRequested;

            foreach (var request in _controller.ActiveRequests)
            {
                OnShowRequested(this, new FileOpeningStatusEventArgs(request));
            }
        }

        private void OnShowRequested(object? sender, FileOpeningStatusEventArgs e)
        {
            if (!_windowsByJobId.TryGetValue(e.Request.JobId, out var window))
            {
                Logger.LogInfo($"Presenting the file opening progress window. JobId: {e.Request.JobId}");
                window = new FileOpeningWindow(e.Request);
                // The user may close the window while the request is still running: drop it and let the next
                // status update present a new one, the state of the request lives in the controller.
                window.Closed += (_, _) => _windowsByJobId.Remove(e.Request.JobId);
                _windowsByJobId[e.Request.JobId] = window;
            }

            window.Activate();
        }

        private void OnDismissRequested(object? sender, FileOpeningStatusEventArgs e)
        {
            if (_windowsByJobId.Remove(e.Request.JobId, out var window))
            {
                window.Close();
            }
        }
    }
}

using CodeArt.MatomoTracking;
using CodeArt.MatomoTracking.Models;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Analytics
{
    internal class MatomoService : IAnalyticsService
    {
        private const string _clickEventCategory = "click";
        private const string _otherEventCategory = "other";
        private const string _productName = "kDrive";
        private const int MaxConcurrentRequests = 4;
        private readonly SemaphoreSlim _throttle = new(MaxConcurrentRequests, MaxConcurrentRequests);
        private readonly IMatomoTracker _tracker;
        private readonly UserDefaults _userDefaults;
        private readonly string? _userAgent;
        private readonly string? _clientHints;
        public MatomoService(IMatomoTracker tracker, UserDefaults userDefaults)
        {
            _tracker = tracker;
            _userDefaults = userDefaults;
            var userAgentInfo = UserAgentBuilder.CurrentDesktopInfo(_productName, AppVersion.Current().Tag, WindowsPlatformVersion.Current());
            _userAgent = NullIfEmpty(UserAgentBuilder.BuildDesktopUserAgent(userAgentInfo));
            _clientHints = NullIfEmpty(UserAgentBuilder.BuildClientHints(userAgentInfo));
        }

        private static string? NullIfEmpty(string value) => string.IsNullOrEmpty(value) ? null : value;

        // Read on each request since the user can change the app language at runtime.
        private static string? CurrentLanguage() => NullIfEmpty(Localizer.Instance.CurrentLanguage);

        public void TrackClick(Keys.Category category, Keys.EventName eventName, int? value = null)
        {
            if (!IsMatomoEnabled())
                return;
            TrackEvent(category, eventName, _clickEventCategory, value);

        }

        public void TrackOther(Keys.Category category, Keys.EventName eventName, int? value = null)
        {
            if (!IsMatomoEnabled())
                return;
            TrackEvent(category, eventName, _otherEventCategory, value);
        }

        private void TrackEvent(Keys.Category category, Keys.EventName eventName, string eventAction, int? value = null)
        {
            if (!IsMatomoEnabled())
                return;

            string? language = CurrentLanguage();
            _ = TrackInBackgroundAsync(() => _tracker.Track(new ClientHintsEventTrackingItem()
            {
                Url = $"kdrive://{category.ToCamelCase()}",
                EventCategory = category.ToCamelCase(),
                EventAction = eventAction,
                EventName = eventName.ToCamelCase(),
                EventValue = value,
                NewVisit = false,
                UserAgent = _userAgent,
                ClientHints = _clientHints,
                Language = language
            }));
        }

        public void TrackPageView(Keys.Category category)
        {
            if (!IsMatomoEnabled())
                return;

            string? language = CurrentLanguage();
            _ = TrackInBackgroundAsync(() => _tracker.Track(new ClientHintsPageViewTrackingItem()
            {
                Url = $"kdrive://{category.ToCamelCase()}",
                ActionName = category.ToCamelCase(),
                UserAgent = _userAgent,
                ClientHints = _clientHints,
                Language = language
            }));
        }

        private async Task TrackInBackgroundAsync(Func<Task> trackAction)
        {
            if (!_throttle.Wait(0))
                return;

            try
            {
                await Task.Run(async () =>
                {
                    try
                    {
                        await trackAction().ConfigureAwait(false);
                    }
                    catch (Exception ex)
                    {
                        Logger.LogWarning($"Unhandled exception while executing background matomo tracking action: {ex}",
                            "MatomoService: Background tracking action failed");
                    }
                }).ConfigureAwait(false);
            }
            finally
            {
                _throttle.Release();
            }
        }

        private bool IsMatomoEnabled()
        {
            var matomoEnabledNode = _userDefaults.GetValue(nameof(AppModel.Settings.MatomoEnabled));
            if (matomoEnabledNode is null)
            {
                Logger.LogInfo("Matomo enabled setting not found in user defaults, skipping initialization.");
                return false;
            }

            if (matomoEnabledNode is JsonValue matomoEnabledValue && matomoEnabledValue.TryGetValue<bool>(out bool matomoEnabled))
            {
                return matomoEnabled;
            }
            else
            {
                Logger.LogWarning("Failed to parse Matomo enabled setting from user defaults.");
                return false;
            }
        }
    }
}

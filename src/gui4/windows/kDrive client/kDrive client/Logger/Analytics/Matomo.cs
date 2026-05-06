using CodeArt.MatomoTracking;
using CodeArt.MatomoTracking.Models;

namespace Infomaniak.kDrive.Analytics
{
    internal class MatomoService : IAnalyticsService
    {
        private const string _clickEventCategory = "click";
        private readonly IMatomoTracker _tracker;
        private readonly UserDefaults _userDefaults;
        public MatomoService(IMatomoTracker tracker, UserDefaults userDefaults)
        {
            _tracker = tracker;
            _userDefaults = userDefaults;
        }

        public void TrackClick(string category, string name, int? value = null)
        {
            if (!IsMatomoEnabled())
                return;

            _tracker.Track(new EventTrackingItem()
            {
                Url = category,
                EventCategory = _clickEventCategory,
                EventName = name,
                EventValue = value
            });
        }

        public void TrackPageView(string category)
        {
            if (!IsMatomoEnabled())
                return;

            _tracker.Track(new PageViewTrackingItem()
            {
                Url = category,
                ActionName = category
            });
        }

        private bool IsMatomoEnabled()
        {
            return _userDefaults.GetValue(nameof(ViewModels.Settings.MatomoEnabled))?.GetValue<bool>() == true;
        }
    }
}

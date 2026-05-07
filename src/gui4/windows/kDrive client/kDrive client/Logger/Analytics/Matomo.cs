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

        public void TrackClick(Keys.Category category, Keys.EventName eventName, int? value = null)
        {
            if (!IsMatomoEnabled())
                return;

            _tracker.Track(new EventTrackingItem()
            {
                Url = category.ToCamelCase(),
                EventCategory = _clickEventCategory,
                EventName = eventName.ToCamelCase(),
                EventValue = value
            });
        }

        public void TrackPageView(Keys.Category category)
        {
            if (!IsMatomoEnabled())
                return;

            _tracker.Track(new PageViewTrackingItem()
            {
                Url = category.ToCamelCase(),
                ActionName = category.ToCamelCase(),
            });
        }

        private bool IsMatomoEnabled()
        {
            return _userDefaults.GetValue(nameof(ViewModels.Settings.MatomoEnabled))?.GetValue<bool>() == true;
        }
    }
}

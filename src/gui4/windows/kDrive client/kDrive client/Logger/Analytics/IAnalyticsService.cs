namespace Infomaniak.kDrive.Analytics
{
    internal interface IAnalyticsService
    {
        void TrackClick(Keys.Category category, Keys.EventName eventName, int? value = null);
        void TrackOther(Keys.Category category, Keys.EventName eventName, int? value = null);
        void TrackPageView(Keys.Category category);

    }
}

namespace Infomaniak.kDrive.Analytics
{
    internal interface IAnalyticsService
    {
        void TrackClick(string category, string name, int? value = null);

        void TrackPageView(string category);

    }
}

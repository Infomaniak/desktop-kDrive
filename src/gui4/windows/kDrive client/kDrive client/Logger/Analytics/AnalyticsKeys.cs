namespace Infomaniak.kDrive.Analytics.Keys
{
    public enum Category
    {
        HomePage,
        ActivityPage,
        StoragePage,
        Search,
        OnboardingWelcomePage,
        OnboardingConnectionFailedPage,
        OnboardingSyncConfigurationPage,
        OnboardingFinalPage,
        DriveSetupDialog
    }

    public enum EventName
    {
        StartSync,
        OpenActivity,
        OpenSignInWeb,
        ShowAllActivities,
        ShowMyActivities,
        OpenErrors,
        OpenItemFolder,
        OpenItemErrorFromIcon,
        OpenItem,
        OpenItemWeb,
        CopyItemWebLink,
        OpenItemError,
        ValidateSearch,
        OpenSignUpWeb,
        ReOpenLoginWeb,
        SelectDrive,
        UnselectDrive,
        OpenAdvancedSettings,
        Confirm,
        Cancel,
        OpenOffersWeb,
        OpenStartFreeWeb,
        ConfigureSync,
        ChangeSyncLocalLocation,
        ChangeSyncExclusions,
        ChangeSyncMode,
    }
}

using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Sentry;
using System;
using System.Runtime.CompilerServices;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Monitoring
{
    internal sealed class SentryMonitoringService : IMonitoringService
    {
        private readonly UserDefaults _userDefaults;
        private readonly MonitoringEventThrottle _eventThrottle = new();
        private readonly object _lock = new();
        private IDisposable? _handler;
        private bool _isFlushing = false;

        public SentryMonitoringService(UserDefaults userDefaults)
        {
            _userDefaults = userDefaults;
        }

        public void AddBreadcrumb(string message, MonitoringEventLevel level)
        {
            lock (_lock)
            {
                if (_handler is null || !IsEnabled())
                    return;

                SentrySdk.AddBreadcrumb(message, level: ToBreadcrumbLevel(level));
            }
        }

        public void CaptureEvent(string title, string message, MonitoringEventLevel level, Exception? exception = null,
            [CallerFilePath] string filePath = "?", [CallerLineNumber] int lineNumber = -1)
        {
            lock (_lock)
            {
                if (_handler is null || !IsEnabled())
                    return;

                if (!String.IsNullOrEmpty(title))
                    AddBreadcrumb(message, level);
                else
                    title = message;

                if (exception is null && !_eventThrottle.TryAcquire(filePath, lineNumber, DateTime.UtcNow))
                {
                    AddBreadcrumb($"Monitoring event throttled for this location, message: {title}", level);
                    return;
                }

                SentrySdk.CaptureEvent(new SentryEvent(exception)
                {
                    Message = title,
                    Level = ToSentryLevel(level)
                });
            }
        }

        public void Start([CallerMemberName] string memberName = "?")
        {
            lock (_lock)
            {
                if (_isFlushing || _handler is not null)
                    return;

                // Sentry's WinUI integration must be initialized outside OnLaunched.
                if (memberName == "OnLaunched")
                {
                    Logger.LogError("Skipping Sentry initialization in OnLaunched to avoid potential issues with Sentry's WinUI integration.");
#if DEBUG
                    throw new InvalidOperationException("Sentry should not be initialized in OnLaunched. Call IMonitoringService.Start() from another point in the application lifecycle.");
#else
                    return;
#endif
                }

                if (!IsEnabled())
                {
                    Logger.LogInfo("Monitoring is disabled or consent is unavailable, skipping initialization.");
                    return;
                }

                string environment = Environment.GetEnvironmentVariable("KDRIVE_SENTRY_ENVIRONMENT") ??
#if DEBUG
                    "dev_unknown";
#else
                    "production";
#endif

                _handler = SentrySdk.Init(options =>
                {
                    options.Dsn = App.Constants.Sentry.Dsn;
                    options.SendDefaultPii = true;
                    options.AutoSessionTracking = true;
                    options.IsGlobalModeEnabled = true;
                    options.Environment = environment;
                });
                App.Current.UnhandledException += CaptureUnhandledException;
            }
        }

        public void Stop()
        {
            IDisposable handler;

            lock (_lock)
            {
                if (_handler is null || _isFlushing)
                    return;

                _isFlushing = true;
                handler = _handler;

                App.Current.UnhandledException -= CaptureUnhandledException;
            }

            _ = StopAsync(handler);
        }

        private async Task StopAsync(IDisposable handler)
        {
            try
            {
                await SentrySdk.FlushAsync(TimeSpan.FromSeconds(5));
            }
            catch (Exception ex)
            {
                Logger.LogError(
                    $"Failed to flush Sentry monitoring service: {ex.Message}",
                    "Monitoring Service Stop Error");
            }
            finally
            {
                try
                {
                    handler.Dispose();
                }
                catch (Exception ex)
                {
                    Logger.LogError(
                        $"Failed to dispose Sentry monitoring service: {ex.Message}",
                        "Monitoring Service Stop Error");
                }

                lock (_lock)
                {
                    if (ReferenceEquals(_handler, handler))
                        _handler = null;

                    _isFlushing = false;
                }
            }
        }

        private bool IsEnabled()
        {
            var appModel = App.ServiceProvider.GetRequiredService<AppModel>();
            if (appModel.IsInitialized)
                return appModel.Settings.SentryEnabled;

            return _userDefaults.GetValue(nameof(Settings.SentryEnabled)) is JsonValue value
                && value.TryGetValue<bool>(out bool enabled) && enabled;
        }

        private void CaptureUnhandledException(object sender, Microsoft.UI.Xaml.UnhandledExceptionEventArgs e)
        {
            CaptureEvent("", e.Exception.Message, MonitoringEventLevel.Error, e.Exception);
        }

        private static BreadcrumbLevel ToBreadcrumbLevel(MonitoringEventLevel level) => level switch
        {
            MonitoringEventLevel.Debug => BreadcrumbLevel.Debug,
            MonitoringEventLevel.Info => BreadcrumbLevel.Info,
            MonitoringEventLevel.Warning => BreadcrumbLevel.Warning,
            MonitoringEventLevel.Error => BreadcrumbLevel.Error,
            MonitoringEventLevel.Fatal => BreadcrumbLevel.Fatal,
            _ => BreadcrumbLevel.Info
        };

        private static SentryLevel ToSentryLevel(MonitoringEventLevel level) => level switch
        {
            MonitoringEventLevel.Debug => SentryLevel.Debug,
            MonitoringEventLevel.Info => SentryLevel.Info,
            MonitoringEventLevel.Warning => SentryLevel.Warning,
            MonitoringEventLevel.Error => SentryLevel.Error,
            MonitoringEventLevel.Fatal => SentryLevel.Fatal,
            _ => SentryLevel.Info
        };
    }
}

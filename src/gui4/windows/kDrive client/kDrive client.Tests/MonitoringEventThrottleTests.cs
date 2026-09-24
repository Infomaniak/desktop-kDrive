using Infomaniak.kDrive.Monitoring;

namespace Infomaniak.kDrive.Tests;

public class MonitoringEventThrottleTests
{
    [Fact]
    public void TryAcquire_AllowsThreeEventsPerMinute()
    {
        var throttle = new MonitoringEventThrottle();
        var now = DateTime.UtcNow;

        Assert.True(throttle.TryAcquire("Logger.cs", 10, now));
        Assert.True(throttle.TryAcquire("Logger.cs", 10, now));
        Assert.True(throttle.TryAcquire("Logger.cs", 10, now));
        Assert.False(throttle.TryAcquire("Logger.cs", 10, now));
        Assert.False(throttle.TryAcquire("Logger.cs", 10, now.AddSeconds(59)));
    }

    [Fact]
    public void TryAcquire_ResetsAfterOneMinute()
    {
        var throttle = new MonitoringEventThrottle();
        var now = DateTime.UtcNow;
        for (int i = 0; i < 3; i++)
            Assert.True(throttle.TryAcquire("Logger.cs", 10, now));

        for (int i = 0; i < 3; i++)
            Assert.True(throttle.TryAcquire("Logger.cs", 10, now.AddMinutes(1)));

        Assert.False(throttle.TryAcquire("Logger.cs", 10, now.AddMinutes(1)));
    }

    [Fact]
    public void TryAcquire_TracksLocationsIndependently()
    {
        var throttle = new MonitoringEventThrottle();
        var now = DateTime.UtcNow;
        for (int i = 0; i < 3; i++)
            Assert.True(throttle.TryAcquire("Logger.cs", 10, now));

        Assert.True(throttle.TryAcquire("Logger.cs", 11, now));
        Assert.True(throttle.TryAcquire("Other.cs", 10, now));
        Assert.False(throttle.TryAcquire("Logger.cs", 10, now));
    }

    [Fact]
    public void TryAcquire_EnforcesLimitAcrossConcurrentCalls()
    {
        var throttle = new MonitoringEventThrottle();
        var now = DateTime.UtcNow;
        int allowed = 0;

        Parallel.For(0, 100, _ =>
        {
            if (throttle.TryAcquire("Logger.cs", 10, now))
                Interlocked.Increment(ref allowed);
        });

        Assert.Equal(3, allowed);
    }
}

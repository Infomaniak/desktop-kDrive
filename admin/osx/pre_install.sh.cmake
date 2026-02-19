#!/bin/sh

# Pre-install script: kill running kDrive processes
# Handles both New Swift GUI (@PACKAGE_APP_EXECUTABLE@) and Legacy Qt GUI (@APPLICATION_EXECUTABLE@)

# Try to kill the new Swift GUI process if running
if pgrep -x "@PACKAGE_APP_EXECUTABLE@" > /dev/null 2>&1; then
    echo "Stopping @PACKAGE_APP_EXECUTABLE@..."
    killall "@PACKAGE_APP_EXECUTABLE@" 2>/dev/null || true
fi

# Also try to kill the legacy Qt GUI process (for upgrades)
if pgrep -x "@APPLICATION_EXECUTABLE@" > /dev/null 2>&1; then
    echo "Stopping legacy @APPLICATION_EXECUTABLE@..."
    killall "@APPLICATION_EXECUTABLE@" 2>/dev/null || true
fi

# Wait a moment for processes to terminate
sleep 2

exit 0

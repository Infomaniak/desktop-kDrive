#!/bin/sh

# kill the old version. see issue #2044
killall @APPLICATION_EXECUTABLE@
# also stop the redesigned (gui4) macOS GUI, which runs as a separate process
killall @APPLICATION_EXECUTABLE@.gui

exit 0

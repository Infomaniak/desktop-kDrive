# [Qt C++ Piwik / Matomo Tracker Library](https://github.com/pbek/qt-piwik-tracker)
[![Build Status Linux/OS X](https://travis-ci.org/pbek/qt-piwik-tracker.svg?branch=master)](https://travis-ci.org/pbek/qt-piwik-tracker)
[![Build Status Windows](https://ci.appveyor.com/api/projects/status/github/pbek/qt-piwik-tracker)](https://ci.appveyor.com/project/pbek/qt-piwik-tracker)
[![Coverage Status](https://coveralls.io/repos/github/pbek/qt-piwik-tracker/badge.svg?branch=master)](https://coveralls.io/github/pbek/qt-piwik-tracker?branch=master)

PiwikTracker is a C++ Qt 5 library for tracking with the open-source analytics 
platform [Piwik, now Matomo](https://matomo.org/).

## Features

- sending visits
- sending events
- sending pings
- custom dimensions can be sent along with the requests 
- language, screen resolution and operating system will be tracked automatically
- client id will be generated and stored automatically

## How to use this library

- checkout the git repository
- include [piwiktracker.pri](https://github.com/pbek/qt-piwik-tracker/blob/master/piwiktracker.pri) 
  to your project like this: `include (qt-piwik-tracker/piwiktracker.pri)`
- include `piwiktracker.h` in your `.cpp` file
- use the library to talk to your Piwik / Matomo server

```cpp
// the 3rd parameter is the site id
PiwikTracker *piwikTracker = new PiwikTracker(qApp, QUrl("https://yourserver"), 1);
piwikTracker->setCustomDimension(1, "some dimension");
piwikTracker->sendVisit("my/path");
```

## References
- [QOwnNotes - cross-platform open source plain-text file markdown note taking](https://www.qownnotes.org)

## Disclaimer
This SOFTWARE PRODUCT is provided by THE PROVIDER "as is" and "with all faults." THE PROVIDER makes no representations or warranties of any kind concerning the safety, suitability, lack of viruses, inaccuracies, typographical errors, or other harmful components of this SOFTWARE PRODUCT. 

There are inherent dangers in the use of any software, and you are solely responsible for determining whether this SOFTWARE PRODUCT is compatible with your equipment and other software installed on your equipment. You are also solely responsible for the protection of your equipment and backup of your data, and THE PROVIDER will not be liable for any damages you may suffer in connection with using, modifying, or distributing this SOFTWARE PRODUCT.

# Infomaniak kDrive app

## The Desktop application for [kDrive by Infomaniak](https://www.infomaniak.com/kdrive).
### Synchronise, share, collaborate.  The Swiss cloud that’s 100% secure.

#### :cloud: All the space you need
Always have access to all your photos, videos and documents. kDrive can store up to 106 TB of data.

#### :globe_with_meridians: A collaborative ecosystem. Everything included. 
Collaborate online on Office documents, organise meetings, share your work. Anything is possible!

#### :lock:  kDrive respects your privacy
Protect your data in a sovereign cloud exclusively developed and hosted in Switzerland. Infomaniak doesn’t analyze or resell your data.

### [Download the kDrive app here](https://www.infomaniak.com/en/apps/download-kdrive)

## License & Contributions
This project is under GPLv3 license.  
If you see a bug or an enhanceable point, feel free to create an issue, so that we can discuss about it, and once approved, we or you (depending on the criticality of the bug/improvement) will take care of the issue and apply a merge request.  
Please, don't do a merge request before creating an issue.

## Tech things
**kDrive Desktop** started as an [Owncloud](https://owncloud.com/) fork in 2019, up until 2023 after all the core functionalities were rewritten  
**LiteSync** is an extension for **Windows** and **macOS** providing on-demand file downloading to save space on your device  

The **kDrive Desktop** application follows the [Syncpal Algorithm by Marius Shekow](https://hal.science/hal-02319573/)

### Language
The project is developed in **C++** using **Qt**.  
The macOS extension is made in **Objective-C/C++**

### Minimum Requirements
| System | With LiteSync | Without LiteSync | ARM
|---|---|---|---|
| Linux | No LiteSync support | KDE / Gnome<br>Ubuntu 19.04, Fedora 38 | KDE / Gnome<br>Ubuntu 19.04, Fedora 38
| macOS | macOS 10.15 | macOS 10.15 | ARM Friendly since 3.3.3<br>no more Rosetta 2
| Windows | x86_64 (64bits), NTFS<br>Windows 10 1709 (October 2017)<br>Windows 11 compatible | Windows 10 1507 (RTM)<br>Windows Server 2016 and up | Windows 11 Insider

### Libraries
The kDrive Desktop Application is using the following libraries :
- The network communications are made using [Poco](https://pocoproject.org/) libraries
- [xxHash](https://xxhash.com/) is a fast Hash algorithm
- We are monitoring the application behaviour with [Sentry](https://sentry.io/)
- [log4cplus](https://github.com/log4cplus/log4cplus) is the logging API to create and populate the log files for the application
- Our Unit-Testing is done with the [CppUnit](https://www.freedesktop.org/wiki/Software/cppunit/) framework module



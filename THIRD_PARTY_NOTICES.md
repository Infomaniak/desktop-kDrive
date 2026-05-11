# Third-Party Notices

This file lists the main third-party components used and/or redistributed by kDrive Desktop.

The exact set of redistributed binaries can vary by platform:

- Conan-managed dependencies are declared in `conanfile.py`.
- Additional packaged dependencies are documented in `infomaniak-build-tools/{linux,macos,windows}/Readme.md` and
  packaging scripts.
- Vendored sources are stored in `src/3rdparty/`.
- Build-only tools and local developer prerequisites are intentionally excluded when they are not redistributed with
  release artifacts.

---

## Conan-Managed Dependencies

### xxHash

- **Version:** 0.8.2
- **License:** BSD-2-Clause
- **Copyright:** Copyright (c) 2012-2021 Yann Collet
- **Repository:** https://github.com/Cyan4973/xxHash
- **Note:** Declared in `conanfile.py`.

### log4cplus

- **Version:** 2.1.2
- **License:** Apache-2.0 OR BSD-2-Clause
- **Copyright:** Copyright (c) 1999-2021 log4cplus project contributors
- **Repository:** https://github.com/log4cplus/log4cplus
- **Note:** Declared in `conanfile.py`.

### zlib

- **Version:** [>=1.2.11 <2]
- **License:** Zlib
- **Copyright:** Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
- **Repository:** https://zlib.net/
- **Note:** Declared in `conanfile.py`; also required by OpenSSL and some platform libzip builds.

### OpenSSL

- **Version:** 3.2.4
- **License:** Apache-2.0
- **Copyright:** Copyright (c) 1998-2024 The OpenSSL Project Authors
- **Repository:** https://www.openssl.org/
- **Note:** Declared in `conanfile.py`. macOS uses the local `openssl-macos/3.2.4` recipe, other platforms use
  `openssl/3.2.4`.

### Qt 6

- **Version:** 6.2.3 / 6.8.3
- **License:** LGPL-3.0 / GPL-2.0 / GPL-3.0 / Commercial
- **Copyright:** Copyright (C) The Qt Company Ltd. and other contributors
- **Repository:** https://www.qt.io/
- **Note:** Declared in `conanfile.py`. This application is built against Qt 6.2.3. Qt is deployed with the application
  on supported platforms. Qt source packages are available from https://download.qt.io/

### Poco

- **Version:** 1.13.3
- **License:** BSL-1.0
- **Copyright:** Copyright (c) 2006-2024, Applied Informatics Software Engineering GmbH and Contributors
- **Repository:** https://github.com/pocoproject/poco
- **Note:** Declared in `conanfile.py`. Required by multiple kDrive libraries and redistributed by platform packaging
  scripts.

### Sentry Native

- **Version:** Build-environment dependent
- **License:** MIT
- **Copyright:** Copyright (c) 2019 Sentry
- **Repository:** https://github.com/getsentry/sentry-native
- **Note:** Declared in `conanfile.py`. Installed outside Conan according to `infomaniak-build-tools/*/Readme.md` and
  redistributed with the application. The repository does not pin one single version across all platforms; the Linux
  setup documentation currently references `0.7.9`.

### Crashpad

- **Version:** Bundled through the Sentry Native crashpad backend
- **License:** BSD-3-Clause
- **Copyright:** Copyright 2014 The Crashpad Authors
- **Repository:** https://chromium.googlesource.com/crashpad/crashpad/
- **Note:** Declared in `conanfile.py`. The `crashpad_handler` executable is redistributed with the application.

---

## Packaged Dependencies Installed Outside Conan

### libzip

- **Version:** 1.10.1
- **License:** BSD-3-Clause
- **Copyright:** Copyright (C) 1999-2024 Dieter Baron and Thomas Klausner
- **Repository:** https://libzip.org/
- **Note:** Used by `libcommon` and `libcommonserver`; redistributed by platform packaging scripts.

### Sparkle

- **Version:** 2.6.4
- **License:** MIT
- **Copyright:** Copyright (c) Sparkle Project
- **Repository:** https://github.com/sparkle-project/Sparkle
- **Note:** macOS only. `Sparkle.framework` is bundled into the application when available.

### CodeArt.MatomoTracking

- **Version:** 1.0.5
- **License:** MIT
- **Copyright:** Copyright (c) CodeArt, Allan Thraen
- **Repository:** https://github.com/CodeArtDK/CodeArt.MatomoTracking
- **Note:** Windows WinUI3 only. Declared as a NuGet dependency in `src/gui4/windows/kDrive client/kDrive client/kDrive client.csproj`.

### H.NotifyIcon.WinUI

- **Version:** 2.3.1
- **License:** MIT
- **Copyright:** Copyright (c) havendv
- **Repository:** https://github.com/HavenDV/H.NotifyIcon
- **Note:** Windows WinUI3 only. Declared as a NuGet dependency in `src/gui4/windows/kDrive client/kDrive client/kDrive client.csproj`.

---

## Vendored Source Dependencies (`src/3rdparty/`)

### utf8proc

- **License:** MIT
- **Additional Notice:** Unicode data license applies to `utf8proc_data.c`
- **Copyright:** Copyright (c) 2014-2021 Steven G. Johnson, Jiahao Chen, Tony Kelman, Jonas Fonseca, and other
  contributors
- **Original Copyright:** Copyright (c) 2009, 2013 Public Software Group e. V., Berlin, Germany
- **Repository:** https://github.com/JuliaStrings/utf8proc
- **Note:** Built from vendored source on Unix platforms.

### keychain

- **License:** MIT
- **Copyright:** Copyright (c) 2019 Hannes Rantzsch, Rene Meusel
- **Repository:** https://github.com/hrantzsch/keychain
- **Note:** Built from vendored source.

### qt-piwik-tracker

- **License:** MIT
- **Copyright:** Copyright (c) 2014-2025 Patrizio Bekerle
- **Repository:** https://github.com/pbek/qt-piwik-tracker
- **Note:** Vendored in `src/3rdparty/` and built as part of `libcommongui`.

### QProgressIndicator

- **License:** MIT
- **Copyright:** Copyright (c) 2011 Morgan Leborgne
- **Note:** Vendored in `src/3rdparty/`. No public upstream repository URL is referenced here because the previously
  used GitHub link is no longer available.

### SQLite

- **License:** Public Domain
- **Copyright:** The author disclaims copyright to the SQLite amalgamation
- **Repository:** https://www.sqlite.org/
- **Note:** Provided by Conan package `sqlite3/3.53.0`.

### QtSingleApplication

- **License:** LGPL-2.1 with Digia Qt LGPL Exception 1.1
- **Copyright:** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies)
- **Repository:** https://github.com/qt-creator/qt-creator
- **Note:** Vendored from Qt Creator sources.

### QtLockedFile

- **License:** LGPL-2.1 with Digia Qt LGPL Exception 1.1
- **Copyright:** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies)
- **Repository:** https://github.com/qt-creator/qt-creator
- **Note:** Vendored from Qt Creator sources; required by QtSingleApplication.

---

## License Texts

### MIT License

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### BSD 2-Clause License

```
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

### BSD 3-Clause License

```
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

### Apache License 2.0

```
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

### Zlib License

```
This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the
use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
   an acknowledgment in the product documentation would be appreciated but is
   not required.

2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.
```

### Boost Software License 1.0

```
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
```

### Unicode Data License

```
COPYRIGHT AND PERMISSION NOTICE

Copyright (c) 1991-2007 Unicode, Inc. All rights reserved. Distributed
under the Terms of Use in http://www.unicode.org/copyright.html.

Permission is hereby granted, free of charge, to any person obtaining a
copy of the Unicode data files and any associated documentation (the "Data
Files") or Unicode software and any associated documentation (the
"Software") to deal in the Data Files or Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, and/or sell copies of the Data Files or Software, and
to permit persons to whom the Data Files or Software are furnished to do
so, provided that (a) the above copyright notice(s) and this permission
notice appear with all copies of the Data Files or Software, (b) both the
above copyright notice(s) and this permission notice appear in associated
documentation, and (c) there is clear notice in each modified Data File or
in the Software as well as in the documentation associated with the Data
File(s) or Software that the data or software has been modified.

THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR
CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THE DATA FILES OR SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall
not be used in advertising or otherwise to promote the sale, use or other
dealings in these Data Files or Software without prior written
authorization of the copyright holder.
```

### Digia Qt LGPL Exception 1.1

```
As an additional permission to the GNU Lesser General Public License version
2.1, the object code form of a "work that uses the Library" may incorporate
material from a header file that is part of the Library. You may distribute
such object code under terms of your choice, provided that:
(i) the header files of the Library have not been modified; and
(ii) the incorporated material is limited to numerical parameters, data
structure layouts, accessors, macros, inline functions and
(iii) templates; and
(iv) you comply with the terms of Section 6 of the GNU Lesser General
Public License version 2.1.

Moreover, you may apply this exception to a modified version of the Library,
provided that such modification does not involve copying material from the
Library into the modified Library's header files unless such material is
limited to (i) numerical parameters; (ii) data structure layouts;
(iii) accessors; and (iv) small macros, templates and inline functions of
five lines or less in length.

Furthermore, you are not required to apply this additional permission to a
modified version of the Library.
```

### SQLite Public Domain Notice

```
The author disclaims copyright to this source code. In place of a legal
notice, here is a blessing:

    May you do good and not evil.
    May you find forgiveness for yourself and forgive others.
    May you share freely, never taking more than you give.
```

### GNU Lesser General Public License v2.1

The full text of the LGPL v2.1 is available in [`src/3rdparty/LICENSE.LGPL`](src/3rdparty/LICENSE.LGPL) and
at: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html

### GNU Lesser General Public License v3.0

The full text of the LGPL v3.0 is available at:
https://www.gnu.org/licenses/lgpl-3.0.html

### GNU General Public License v2.0

The full text of the GPL v2.0 is available at:
https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

### GNU General Public License v3.0

The full text of the GPL v3.0 is available at:
https://www.gnu.org/licenses/gpl-3.0.html

---

## Notes

- This file accompanies the kDrive Desktop application.
- The source code for this application is available at: https://github.com/Infomaniak/desktop-kdrive

/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "libcommon/utility/utility.h"

#include "CrashReporterConfig.h"

#include <libcrashreporter-gui/CrashReporter.h>

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

#ifdef Q_OS_LINUX
    if (app.arguments().size() != 8) {
#else
    if (app.arguments().size() != 2) {
#endif
        qDebug() << "Invalid number of arguments";
        return 1;
    }

    KDC::CommonUtility::setupTranslations(&app, KDC::LanguageDefault);

    // TODO: install socorro ....
    CrashReporter reporter(QUrl(CRASHREPORTER_SUBMIT_URL), app.arguments());

#ifdef CRASHREPORTER_ICON
    reporter.setLogo(QPixmap(CRASHREPORTER_ICON));
#endif
    reporter.setWindowTitle(CRASHREPORTER_PRODUCT_NAME);
    reporter.setText(app.translate("CrashReporter",
                                   "<html><head/><body><p><span style=\"font-weight:600;\">Sorry!</span> %1 crashed."
                                   " Please tell us about it!"
                                   " %1 has created an error report for you that can help improve the stability of the product."
                                   " You can now send this report directly to the developers.</p></body></html>")
                         .arg(CRASHREPORTER_PRODUCT_NAME));

    reporter.setReportData("BuildID", CRASHREPORTER_BUILD_ID);
    reporter.setReportData("ProductName", CRASHREPORTER_PRODUCT_NAME);
    reporter.setReportData("Version", CRASHREPORTER_VERSION_STRING);
    reporter.setReportData("ReleaseChannel", CRASHREPORTER_RELEASE_CHANNEL);

    // reporter.setReportData( "timestamp", QByteArray::number( QDateTime::currentDateTime().toTime_t() ) );


    // add parameters

    //            << Pair("InstallTime", "1357622062")
    //            << Pair("Theme", "classic/1.0")
    //            << Pair("Version", "30")
    //            << Pair("id", "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}")
    //            << Pair("Vendor", "Mozilla")
    //            << Pair("EMCheckCompatibility", "true")
    //            << Pair("Throttleable", "0")
    //            << Pair("URL", "http://code.google.com/p/crashme/")
    //            << Pair("version", "20.0a1")
    //            << Pair("CrashTime", "1357770042")
    //            << Pair("submitted_timestamp", "2013-01-09T22:21:18.646733+00:00")
    //            << Pair("buildid", "20130107030932")
    //            << Pair("timestamp", "1357770078.646789")
    //            << Pair("Notes", "OpenGL: NVIDIA Corporation -- GeForce 8600M GT/PCIe/SSE2 -- 3.3.0 NVIDIA 313.09 --
    //            texture_from_pixmap\r\n")
    //            << Pair("StartupTime", "1357769913")
    //            << Pair("FramePoisonSize", "4096")
    //            << Pair("FramePoisonBase", "7ffffffff0dea000")
    //            << Pair("Add-ons", "%7B972ce4c6-7e08-4474-a285-3208198ce6fd%7D:20.0a1,crashme%40ted.mielczarek.org:0.4")
    //            << Pair("SecondsSinceLastCrash", "1831736")
    //            << Pair("ProductName", "WaterWolf")
    //            << Pair("legacy_processing", "0")
    //            << Pair("ProductID", "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}")

    ;

    // TODO:
    // send log
    //    QFile logFile( INSERT_FILE_PATH_HERE );
    //    logFile.open( QFile::ReadOnly );
    //    reporter.setReportData( "upload_file_kdrivelog", qCompress( logFile.readAll() ), "application/x-gzip", QFileInfo(
    //    INSERT_FILE_PATH_HERE ).fileName().toUtf8()); logFile.close();

    reporter.show();

    return app.exec();
}

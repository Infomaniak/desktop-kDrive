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

#include "utility.h"

#include <QCollator>

#if defined(Q_OS_WIN)
#include "utility_win.cpp"
#elif defined(Q_OS_MAC)
#include "utility_mac.cpp"
#else
#include "utility_linux.cpp"
#endif

namespace KDC {
struct Period {
        const char *name;
        quint64 msec;

        QString description(quint64 value) const { return QCoreApplication::translate("Utility", name, 0, value); }
};
// QTBUG-3945 and issue #4855: QT_TRANSLATE_NOOP does not work with plural form because lupdate
// limitation unless we fake more arguments
// (it must be in the form ("context", "source", "comment", n)
#undef QT_TRANSLATE_NOOP
#define QT_TRANSLATE_NOOP(ctx, str, ...) str
Q_DECL_CONSTEXPR Period periods[] = {{QT_TRANSLATE_NOOP("Utility", "%n year(s)", 0, _), 365 * 24 * 3600 * 1000LL},
                                     {QT_TRANSLATE_NOOP("Utility", "%n month(s)", 0, _), 30 * 24 * 3600 * 1000LL},
                                     {QT_TRANSLATE_NOOP("Utility", "%n day(s)", 0, _), 24 * 3600 * 1000LL},
                                     {QT_TRANSLATE_NOOP("Utility", "%n hour(s)", 0, _), 3600 * 1000LL},
                                     {QT_TRANSLATE_NOOP("Utility", "%n minute(s)", 0, _), 60 * 1000LL},
                                     {QT_TRANSLATE_NOOP("Utility", "%n second(s)", 0, _), 1000LL},
                                     {0, 0}};

void CommonGuiUtility::sleep(int sec) {
    QThread::sleep(sec);
}

QString CommonGuiUtility::durationToDescriptiveString1(quint64 msecs) {
    quint64 twoDaysValue = 2 * 24 * 3600 * 1000LL;
    if (msecs > twoDaysValue) {
        msecs = twoDaysValue;
    }

    int p = 0;
    while (periods[p + 1].name && msecs < periods[p].msec) {
        p++;
    }

    quint64 amount = qRound(double(msecs) / periods[p].msec);
    return periods[p].description(amount);
}

void CommonGuiUtility::setupFavLink(const QString &folder) {
    KDC::setupFavLink_private(folder);
}

bool compareSubfolders(const KDC::NodeInfo &s1, const KDC::NodeInfo &s2) {
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    return collator.compare(s1.name(), s2.name()) < 0;
}

void CommonGuiUtility::sortSubfolders(QList<::KDC::NodeInfo> subfolders) {
    std::sort(subfolders.begin(), subfolders.end(), compareSubfolders);
}

QString CommonGuiUtility::octetsToString(qint64 octets) {
#define THE_FACTOR 1024
    static const qint64 kb = THE_FACTOR;
    static const qint64 mb = THE_FACTOR * kb;
    static const qint64 gb = THE_FACTOR * mb;

    QString s;
    qreal value = octets;

    // Whether we care about decimals: only for GB/MB and only
    // if it's less than 10 units.
    bool round = true;

    // do not display terra byte with the current units, as when
    // the MB, GB and KB units were made, there was no TB,
    // see the JEDEC standard
    // https://en.wikipedia.org/wiki/JEDEC_memory_standards
    if (octets >= gb) {
        s = QCoreApplication::translate("Utility", "%L1 GB");
        value /= gb;
        round = false;
    } else if (octets >= mb) {
        s = QCoreApplication::translate("Utility", "%L1 MB");
        value /= mb;
        round = false;
    } else if (octets >= kb) {
        s = QCoreApplication::translate("Utility", "%L1 KB");
        value /= kb;
    } else {
        s = QCoreApplication::translate("Utility", "%L1 B");
    }

    if (value > 9.95) round = true;

    if (round) return s.arg(qRound(value));

    return s.arg(value, 0, 'g', 2);
}

} // namespace KDC

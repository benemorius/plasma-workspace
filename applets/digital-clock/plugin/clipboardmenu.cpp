/*
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "clipboardmenu.h"
#include "klocalizedstring.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QRegularExpression>

ClipboardMenu::ClipboardMenu(QObject *parent)
    : QObject(parent)
{
}

ClipboardMenu::~ClipboardMenu() = default;

QDateTime ClipboardMenu::currentDate() const
{
    return m_currentDate;
}

void ClipboardMenu::setCurrentDate(const QDateTime &currentDate)
{
    if (m_currentDate != currentDate) {
        m_currentDate = currentDate;
        Q_EMIT currentDateChanged();
    }
}

bool ClipboardMenu::secondsIncluded() const
{
    return m_secondsIncluded;
}

void ClipboardMenu::setSecondsIncluded(bool secondsIncluded)
{
    if (m_secondsIncluded != secondsIncluded) {
        m_secondsIncluded = secondsIncluded;
        Q_EMIT secondsIncludedChanged();
    }
}

void ClipboardMenu::setupMenu(QAction *action)
{
    QMenu *menu = new QMenu;

    /*
     * The refresh rate of m_currentDate depends of what is shown in the plasmoid.
     * If only minutes are shown, the value is updated at the full minute and
     * seconds are always 0 or 59 and thus useless/confusing to offer for copy.
     * Use a reference to the config's showSeconds to decide if seconds are sent
     * to the clipboard. There was no workaround found ...
     */
    connect(menu, &QMenu::aboutToShow, this, [this, menu] {
        menu->clear();

        const QDate date = m_currentDate.date();
        const QTime time = m_currentDate.time();
        const QRegularExpression rx(QStringLiteral("[^0-9:]"));
        const QChar ws = QLatin1Char(' ');
        QString s;
        QAction *a;

        // e.g 12:30 PM or 12:30:01 PM
        s = m_secondsIncluded ? QLocale::system().toString(time, QLocale::LongFormat) : QLocale::system().toString(time, QLocale::ShortFormat);
        a = menu->addAction(s);
        a->setData(s);

        // the same as the option above but shows the opposite of the "show seconds" setting
        // e.g if "show seconds" is enabled it will show the time without seconds and vice-versa
        s = m_secondsIncluded ? QLocale::system().toString(time, QLocale::ShortFormat) : QLocale::system().toString(time, QLocale::LongFormat);
        a = menu->addAction(s);
        a->setData(s);

        // e.g 4/28/22
        s = QLocale::system().toString(date, QLocale::ShortFormat);
        a = menu->addAction(s);
        a->setData(s);

        // e.g Thursday, April 28, 2022
        s = QLocale::system().toString(date, QLocale::LongFormat);
        a = menu->addAction(s);
        a->setData(s);

        // e.g Thursday, April 28, 2022 12:30 PM or Thursday, April 28, 2022 12:30:01 PM -03
        s = m_secondsIncluded ? QLocale::system().toString(date, QLocale::LongFormat) + ws + QLocale::system().toString(time, QLocale::LongFormat) : QLocale::system().toString(date, QLocale::LongFormat) + ws + QLocale::system().toString(time, QLocale::ShortFormat);
        a = menu->addAction(s);
        a->setData(s);

        // e.g 2022-04-28
        s = date.toString(Qt::ISODate);
        a = menu->addAction(s);
        a->setData(s);

        // e.g 2022-04-28 12:30 PM or 2022-04-28 12:30:01 PM -03
        s = m_secondsIncluded ? date.toString(Qt::ISODate) + ws + QLocale::system().toString(time, QLocale::LongFormat) : date.toString(Qt::ISODate) + ws + QLocale::system().toString(time, QLocale::ShortFormat);
        a = menu->addAction(s);
        a->setData(s);

        menu->addSeparator();

        QMenu *otherCalendarsMenu = menu->addMenu(i18n("Other Calendars"));

        /* Add ICU Calendars if QLocale is ready for
                Chinese, Coptic, Ethiopic, (Gregorian), Hebrew, Indian, Islamic, Persian

                otherCalendarsMenu->addSeparator();
        */
        s = QString::number(m_currentDate.toMSecsSinceEpoch() / 1000);
        a = otherCalendarsMenu->addAction(i18nc("unix timestamp (seconds since 1.1.1970)", "%1 (UNIX Time)", s));
        a->setData(s);
        s = QString::number(qreal(2440587.5) + qreal(m_currentDate.toMSecsSinceEpoch()) / qreal(86400000), 'f', 5);
        a = otherCalendarsMenu->addAction(i18nc("for astronomers (days and decimals since ~7000 years ago)", "%1 (Julian Date)", s));
        a->setData(s);
    });

    connect(menu, &QMenu::triggered, menu, [](QAction *action) {
        qApp->clipboard()->setText(action->data().toString());
        qApp->clipboard()->setText(action->data().toString(), QClipboard::Selection);
    });

    // QMenu cannot have QAction as parent and setMenu doesn't take ownership
    connect(action, &QObject::destroyed, menu, &QObject::deleteLater);

    action->setMenu(menu);
}

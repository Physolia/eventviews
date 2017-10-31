/*
  This file is part of KOrganizer.

  Copyright (c) 2001 Cornelius Schumacher <schumacher@kde.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

  As a special exception, permission is given to link this program
  with any edition of Qt, and distribute the resulting executable,
  without including the source code for Qt in the source distribution.
*/

#include "whatsnextview.h"

#include <Akonadi/Calendar/ETMCalendar>
#include <CalendarSupport/KCalPrefs>
#include <CalendarSupport/Utils>

#include <KCalUtils/IncidenceFormatter>

#include <KIconLoader>

#include <QBoxLayout>

using namespace EventViews;

void WhatsNextTextBrowser::setSource(const QUrl &name)
{
    QString uri = name.toString();
    if (uri.startsWith(QStringLiteral("event:"))) {
        Q_EMIT showIncidence(uri);
    } else if (uri.startsWith(QStringLiteral("todo:"))) {
        Q_EMIT showIncidence(uri);
    } else {
        QTextBrowser::setSource(QUrl(uri));
    }
}

WhatsNextView::WhatsNextView(QWidget *parent)
    : EventView(parent)
{
    mView = new WhatsNextTextBrowser(this);
    connect(mView, &WhatsNextTextBrowser::showIncidence, this, &WhatsNextView::showIncidence);

    QBoxLayout *topLayout = new QVBoxLayout(this);
    topLayout->setMargin(0);
    topLayout->addWidget(mView);
}

WhatsNextView::~WhatsNextView()
{
}

int WhatsNextView::currentDateCount() const
{
    return mStartDate.daysTo(mEndDate);
}

void WhatsNextView::createTaskRow(KIconLoader *kil)
{
    QString ipath;
    kil->loadIcon(QStringLiteral("view-calendar-tasks"), KIconLoader::NoGroup, 22,
                  KIconLoader::DefaultState, QStringList(), &ipath);
    mText += QLatin1String("<h2><img src=\"");
    mText += ipath;
    mText += QLatin1String("\" width=\"22\" height=\"22\">");
    mText += i18n("To-dos:") + QLatin1String("</h2>\n");
    mText += QLatin1String("<ul>\n");
}

void WhatsNextView::updateView()
{
    KIconLoader *kil = KIconLoader::global();
    QString ipath;
    kil->loadIcon(QStringLiteral("office-calendar"), KIconLoader::NoGroup, 32,
                  KIconLoader::DefaultState, QStringList(), &ipath);

    mText = QStringLiteral("<table width=\"100%\">\n");
    mText += QLatin1String("<tr bgcolor=\"#3679AD\"><td><h1>");
    mText += QLatin1String("<img src=\"");
    mText += ipath;
    mText += QLatin1String("\" width=\"32\" height=\"32\">");
    mText += QLatin1String("<font color=\"white\"> ");
    mText += i18n("What's Next?") + QLatin1String("</font></h1>");
    mText += QLatin1String("</td></tr>\n<tr><td>");

    mText += QLatin1String("<h2>");
    if (mStartDate.daysTo(mEndDate) < 1) {
        mText += QLocale::system().toString(mStartDate);
    } else {
        mText += i18nc(
            "date from - to", "%1 - %2",
            QLocale::system().toString(mStartDate),
            QLocale::system().toString(mEndDate));
    }
    mText += QLatin1String("</h2>\n");

    KCalCore::Event::List events;
    events = calendar()->events(mStartDate, mEndDate, QTimeZone::systemTimeZone(), false);
    events = calendar()->sortEvents(events, KCalCore::EventSortStartDate,
                                    KCalCore::SortDirectionAscending);

    if (!events.isEmpty()) {
        mText += QLatin1String("<p></p>");
        kil->loadIcon(QStringLiteral("view-calendar-day"), KIconLoader::NoGroup, 22,
                      KIconLoader::DefaultState, QStringList(), &ipath);
        mText += QLatin1String("<h2><img src=\"");
        mText += ipath;
        mText += QLatin1String("\" width=\"22\" height=\"22\">");
        mText += i18n("Events:") + QLatin1String("</h2>\n");
        mText += QLatin1String("<table>\n");
        Q_FOREACH (const KCalCore::Event::Ptr &ev, events) {
            if (!ev->recurs()) {
                appendEvent(ev);
            } else {
                KCalCore::Recurrence *recur = ev->recurrence();
                int duration = ev->dtStart().secsTo(ev->dtEnd());
                QDateTime start
                    = recur->getPreviousDateTime(QDateTime(mStartDate, QTime(), Qt::LocalTime));
                QDateTime end = start.addSecs(duration);
                QDateTime endDate(mEndDate, QTime(23, 59, 59), Qt::LocalTime);
                if (end.date() >= mStartDate) {
                    appendEvent(ev, start.toLocalTime(), end.toLocalTime());
                }
                const auto times = recur->timesInInterval(start, endDate);
                int count = times.count();
                if (count > 0) {
                    int i = 0;
                    if (times[0] == start) {
                        ++i;  // start has already been appended
                    }
                    if (!times[count - 1].isValid()) {
                        --count;  // list overflow
                    }
                    for (; i < count && times[i].date() <= mEndDate; ++i) {
                        appendEvent(ev, times[i].toLocalTime());
                    }
                }
            }
        }
        mText += QLatin1String("</table>\n");
    }

    mTodos.clear();
    KCalCore::Todo::List todos = calendar()->todos(KCalCore::TodoSortDueDate,
                                                   KCalCore::SortDirectionAscending);
    if (!todos.isEmpty()) {
        bool taskHeaderWasCreated = false;
        Q_FOREACH (const KCalCore::Todo::Ptr &todo, todos) {
            if (!todo->isCompleted() && todo->hasDueDate() && todo->dtDue().date() <= mEndDate) {
                if (!taskHeaderWasCreated) {
                    createTaskRow(kil);
                    taskHeaderWasCreated = true;
                }
                appendTodo(todo);
            }
        }
        bool gotone = false;
        int priority = 1;
        while (!gotone && priority <= 9) {
            Q_FOREACH (const KCalCore::Todo::Ptr &todo, todos) {
                if (!todo->isCompleted() && (todo->priority() == priority)) {
                    if (!taskHeaderWasCreated) {
                        createTaskRow(kil);
                        taskHeaderWasCreated = true;
                    }
                    appendTodo(todo);
                    gotone = true;
                }
            }
            priority++;
        }
        if (taskHeaderWasCreated) {
            mText += QLatin1String("</ul>\n");
        }
    }

    QStringList myEmails(CalendarSupport::KCalPrefs::instance()->allEmails());
    int replies = 0;
    events = calendar()->events(QDate::currentDate(), QDate(2975, 12, 6),
                                QTimeZone::systemTimeZone());
    Q_FOREACH (const KCalCore::Event::Ptr &ev, events) {
        KCalCore::Attendee::Ptr me = ev->attendeeByMails(myEmails);
        if (me != nullptr) {
            if (me->status() == KCalCore::Attendee::NeedsAction && me->RSVP()) {
                if (replies == 0) {
                    mText += QLatin1String("<p></p>");
                    kil->loadIcon(QStringLiteral("mail-reply-sender"), KIconLoader::NoGroup, 22,
                                  KIconLoader::DefaultState, QStringList(), &ipath);
                    mText += QLatin1String("<h2><img src=\"");
                    mText += ipath;
                    mText += QLatin1String("\" width=\"22\" height=\"22\">");
                    mText += i18n("Events and to-dos that need a reply:")
                             + QLatin1String("</h2>\n");
                    mText += QLatin1String("<table>\n");
                }
                replies++;
                appendEvent(ev);
            }
        }
    }
    todos = calendar()->todos();
    Q_FOREACH (const KCalCore::Todo::Ptr &to, todos) {
        KCalCore::Attendee::Ptr me = to->attendeeByMails(myEmails);
        if (me != nullptr) {
            if (me->status() == KCalCore::Attendee::NeedsAction && me->RSVP()) {
                if (replies == 0) {
                    mText += QLatin1String("<p></p>");
                    kil->loadIcon(QStringLiteral("mail-reply-sender"), KIconLoader::NoGroup, 22,
                                  KIconLoader::DefaultState, QStringList(), &ipath);
                    mText += QLatin1String("<h2><img src=\"");
                    mText += ipath;
                    mText += QLatin1String("\" width=\"22\" height=\"22\">");
                    mText += i18n("Events and to-dos that need a reply:")
                             + QLatin1String("</h2>\n");
                    mText += QLatin1String("<table>\n");
                }
                replies++;
                appendEvent(to);
            }
        }
    }
    if (replies > 0) {
        mText += QLatin1String("</table>\n");
    }

    mText += QLatin1String("</td></tr>\n</table>\n");

    mView->setText(mText);
}

void WhatsNextView::showDates(const QDate &start, const QDate &end, const QDate &)
{
    mStartDate = start;
    mEndDate = end;
    updateView();
}

void WhatsNextView::showIncidences(const Akonadi::Item::List &incidenceList, const QDate &date)
{
    Q_UNUSED(incidenceList);
    Q_UNUSED(date);
}

void WhatsNextView::changeIncidenceDisplay(const Akonadi::Item &,
                                           Akonadi::IncidenceChanger::ChangeType)
{
    updateView();
}

void WhatsNextView::appendEvent(const KCalCore::Incidence::Ptr &incidence, const QDateTime &start,
                                const QDateTime &end)
{
    mText += QLatin1String("<tr><td><b>");
    if (const KCalCore::Event::Ptr event = incidence.dynamicCast<KCalCore::Event>()) {
        auto starttime = start.toLocalTime();
        if (!starttime.isValid()) {
            starttime = event->dtStart().toLocalTime();
        }
        auto endtime = end.toLocalTime();
        if (!endtime.isValid()) {
            endtime = starttime.addSecs(event->dtStart().secsTo(event->dtEnd()));
        }

        if (starttime.date().daysTo(endtime.date()) >= 1) {
            if (event->allDay()) {
                mText
                    += i18nc("date from - to", "%1 - %2",
                             QLocale().toString(starttime.date(), QLocale::ShortFormat),
                             QLocale().toString(endtime.date(), QLocale::ShortFormat));
            } else {
                mText
                    += i18nc("date from - to", "%1 - %2", QLocale().toString(starttime,
                                                                             QLocale::ShortFormat),
                             QLocale().toString(endtime, QLocale::ShortFormat));
            }
        } else {
            if (event->allDay()) {
                mText += QLocale().toString(starttime.date(), QLocale::ShortFormat);
            } else {
                mText += i18nc("date, from - to", "%1, %2 - %3",
                               QLocale().toString(starttime.date(), QLocale::ShortFormat),
                               QLocale().toString(starttime.time(), QLocale::ShortFormat),
                               QLocale().toString(endtime.time(), QLocale::ShortFormat));
            }
        }
    }
    mText += QLatin1String("</b></td><td><a ");
    if (incidence->type() == KCalCore::Incidence::TypeEvent) {
        mText += QLatin1String("href=\"event:");
    }
    if (incidence->type() == KCalCore::Incidence::TypeTodo) {
        mText += QLatin1String("href=\"todo:");
    }
    mText += incidence->uid() + QLatin1String("\">");
    mText += incidence->summary();
    mText += QLatin1String("</a></td></tr>\n");
}

void WhatsNextView::appendTodo(const KCalCore::Incidence::Ptr &incidence)
{
    Akonadi::Item aitem = calendar()->item(incidence);
    if (mTodos.contains(aitem)) {
        return;
    }
    mTodos.append(aitem);
    mText += QLatin1String("<li><a href=\"todo:") + incidence->uid() + QLatin1String("\">");
    mText += incidence->summary();
    mText += QLatin1String("</a>");

    if (const KCalCore::Todo::Ptr todo = CalendarSupport::todo(aitem)) {
        if (todo->hasDueDate()) {
            mText += i18nc("to-do due date", "  (Due: %1)",
                           KCalUtils::IncidenceFormatter::dateTimeToString(
                               todo->dtDue(), todo->allDay()));
        }
        mText += QLatin1String("</li>\n");
    }
}

void WhatsNextView::showIncidence(const QString &uid)
{
    Akonadi::Item item;

    Akonadi::ETMCalendar::Ptr cal = calendar();
    if (!cal) {
        return;
    }

    if (uid.startsWith(QStringLiteral("event:"))) {
        item = cal->item(uid.mid(6));
    } else if (uid.startsWith(QStringLiteral("todo:"))) {
        item = cal->item(uid.mid(5));
    }

    if (item.isValid()) {
        Q_EMIT showIncidenceSignal(item);
    }
}

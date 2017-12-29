/*----------------------------------------------------------------------*
 * Arduino Timezone Library v1.0                                        *
 * Jack Christensen Mar 2012                                            *
 *                                                                      *
 * This work is licensed under the Creative Commons Attribution-        *
 * ShareAlike 3.0 Unported License. To view a copy of this license,     *
 * visit http://creativecommons.org/licenses/by-sa/3.0/ or send a       *
 * letter to Creative Commons, 171 Second Street, Suite 300,            *
 * San Francisco, California, 94105, USA.                               *
 *----------------------------------------------------------------------*/

//#include <pgmspace.h>
#include "Timezone.h"

/*----------------------------------------------------------------------*
 * Create a Timezone object from the given time change rules.           *
 *----------------------------------------------------------------------*/
Timezone::Timezone(TimeChangeRule dstStart, TimeChangeRule stdStart)
{
    _dst = dstStart;
    _std = stdStart;
}

/*----------------------------------------------------------------------*
 * Convert the given UTC time to local time, standard or                *
 * daylight time, as appropriate.                                       *
 *----------------------------------------------------------------------*/
time_t Timezone::toLocal(time_t utc)
{
    //recalculate the time change points if needed
    if (year(utc) != year(_dstUTC)) calcTimeChanges(year(utc));

    if (utcIsDST(utc))
        return utc + _dst.offset * SECS_PER_MIN;
    else
        return utc + _std.offset * SECS_PER_MIN;
}

/*----------------------------------------------------------------------*
 * Determine whether the given UTC time_t is within the DST interval    *
 * or the Standard time interval.                                       *
 *----------------------------------------------------------------------*/
bool Timezone::utcIsDST(time_t utc)
{
    //recalculate the time change points if needed
    if (year(utc) != year(_dstUTC)) calcTimeChanges(year(utc));

    if (_stdUTC > _dstUTC)    //northern hemisphere
        return (utc >= _dstUTC && utc < _stdUTC);
    else                      //southern hemisphere
        return !(utc >= _stdUTC && utc < _dstUTC);
}

/*----------------------------------------------------------------------*
 * Determine whether the given Local time_t is within the DST interval  *
 * or the Standard time interval.                                       *
 *----------------------------------------------------------------------*/
bool Timezone::locIsDST(time_t local)
{
    //recalculate the time change points if needed
    if (year(local) != year(_dstLoc)) calcTimeChanges(year(local));

    if (_stdLoc > _dstLoc)    //northern hemisphere
        return (local >= _dstLoc && local < _stdLoc);
    else                      //southern hemisphere
        return !(local >= _stdLoc && local < _dstLoc);
}


/*----------------------------------------------------------------------*
 * Calculate the DST and standard time change points for the given      *
 * given year as local and UTC time_t values.                           *
 *----------------------------------------------------------------------*/
void Timezone::calcTimeChanges(int yr)
{
    _dstLoc = toTime_t(_dst, yr);
    _stdLoc = toTime_t(_std, yr);
    _dstUTC = _dstLoc - _std.offset * SECS_PER_MIN;
    _stdUTC = _stdLoc - _dst.offset * SECS_PER_MIN;
}

/*----------------------------------------------------------------------*
 * Convert the given DST change rule to a time_t value                  *
 * for the given year.                                                  *
 *----------------------------------------------------------------------*/
time_t Timezone::toTime_t(TimeChangeRule r, int yr)
{
    tmElements_t tm;
    time_t t;
    uint8_t m, w;            //temp copies of r.month and r.week

    m = r.month;
    w = r.week;
    if (w == 0) {            //Last week = 0
        if (++m > 12) {      //for "Last", go to the next month
            m = 1;
            yr++;
        }
        w = 1;               //and treat as first week of next month, subtract 7 days later
    }

    tm.Hour = r.hour;
    tm.Minute = 0;
    tm.Second = 0;
    tm.Day = 1;
    tm.Month = m;
    tm.Year = yr - 1970;
    t = makeTime(tm);        //first day of the month, or first day of next month for "Last" rules
    t += (7 * (w - 1) + (r.dow - weekday(t) + 7) % 7) * SECS_PER_DAY;
    if (r.week == 0) t -= 7 * SECS_PER_DAY;    //back up a week if this is a "Last" rule
    return t;
}
/**
 * @file build_time.h
 * @brief Build timestamp utilities
 *
 * Captures compile-time timestamp and provides conversion to struct tm
 */

#ifndef BUILD_TIME_H
#define BUILD_TIME_H

#include <time.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Convert __DATE__ and __TIME__ macros to struct tm
     *
     * @param date Compile date string from __DATE__ (e.g., "Jan 10 2026")
     * @param time Compile time string from __TIME__ (e.g., "14:23:45")
     * @param tm_out Output struct tm
     * @return 0 on success, -1 on error
     */
    static inline int build_time_to_tm(const char *date, const char *time, struct tm *tm_out)
    {
        if (!date || !time || !tm_out)
        {
            return -1;
        }

        memset(tm_out, 0, sizeof(struct tm));

        // Parse time (HH:MM:SS)
        int hour, min, sec;
        if (sscanf(time, "%d:%d:%d", &hour, &min, &sec) != 3)
        {
            return -1;
        }
        tm_out->tm_hour = hour;
        tm_out->tm_min = min;
        tm_out->tm_sec = sec;

        // Parse date (MMM DD YYYY)
        char month_str[4] = {0};
        int day, year;
        if (sscanf(date, "%3s %d %d", month_str, &day, &year) != 3)
        {
            return -1;
        }

        // Convert month string to number
        const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        int month = -1;
        for (int i = 0; i < 12; i++)
        {
            if (strcmp(month_str, months[i]) == 0)
            {
                month = i;
                break;
            }
        }

        if (month == -1)
        {
            return -1;
        }

        tm_out->tm_mon = month;
        tm_out->tm_mday = day;
        tm_out->tm_year = year - 1900; // tm_year is years since 1900
        tm_out->tm_wday = 0;           // Not calculated, RTC will handle
        tm_out->tm_yday = 0;           // Not calculated
        tm_out->tm_isdst = -1;         // Not known

        return 0;
    }

    /**
     * @brief Get build timestamp as struct tm
     *
     * Uses __DATE__ and __TIME__ compiler macros to capture compile time
     *
     * @param tm_out Output struct tm with build timestamp
     * @return 0 on success, -1 on error
     */
    static inline int get_build_time(struct tm *tm_out)
    {
        return build_time_to_tm(__DATE__, __TIME__, tm_out);
    }

#ifdef __cplusplus
}
#endif

#endif // BUILD_TIME_H

#include "tz.h"

#include <cstdlib>
#include <ctime>

namespace irrigation_core {

void tz_set(const char *tz) {
  setenv("TZ", tz, 1);
  tzset();
}

LocalTime local_from_epoch(Epoch utc) {
  LocalTime out;
  if (utc < 0)
    return out;  // invalid == false

  const time_t t = static_cast<time_t>(utc);
  struct tm tm {};
  if (localtime_r(&t, &tm) == nullptr)
    return out;

  out.year = static_cast<int16_t>(tm.tm_year + 1900);
  out.month = static_cast<uint8_t>(tm.tm_mon + 1);
  out.day = static_cast<uint8_t>(tm.tm_mday);
  out.hour = static_cast<uint8_t>(tm.tm_hour);
  out.minute = static_cast<uint8_t>(tm.tm_min);
  out.second = static_cast<uint8_t>(tm.tm_sec);
  out.weekday = static_cast<uint8_t>(tm.tm_wday);
  // Computed from the civil fields, NOT from utc/86400: on the two DST days a
  // local civil day is 23 or 25 hours long, so dividing the epoch would slew
  // the day boundary and let an idempotency key repeat or skip.
  out.day_number = days_from_civil(out.year, out.month, out.day);
  out.valid = out.year >= MIN_TRUSTED_YEAR;
  return out;
}

Epoch epoch_from_local(int year, int month, int day, int hour, int minute, int second) {
  struct tm tm {};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
  tm.tm_isdst = -1;  // let the C library decide; see the header's note
  return static_cast<Epoch>(mktime(&tm));
}

}  // namespace irrigation_core

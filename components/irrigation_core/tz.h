#pragma once
//
//  The one place civil-time conversion happens.
//
//  We deliberately delegate the timezone rules to the C library rather than
//  re-implementing them. The property under test is "does the resolver behave
//  correctly across a DST transition", not "did we reimplement the IANA
//  database correctly" -- and libc's tz handling is the same code that runs on
//  the device, since ESP-IDF's newlib uses the TZ environment variable exactly
//  as glibc/musl do. Testing against it therefore tests the real thing.
//
#include "types.h"

namespace irrigation_core {

// Converts a UTC epoch to civil time in the *process* timezone (TZ env var).
// On device ESPHome's `time:` component has already called setenv("TZ", ...)
// and tzset(); on the host, tz_set_or_die() below does it.
//
// Returns valid == false for epochs ESPHome would consider untrustworthy.
LocalTime local_from_epoch(Epoch utc);

// The inverse, for tests and for computing a program's next start instant.
// `is_dst` is resolved by the C library; for a local time that does not exist
// (the spring-forward gap) or that exists twice (the autumn overlap), mktime's
// normalisation applies -- which is exactly why FR-5.9 forbids start times in
// 01:30-03:30 rather than trying to define behaviour there.
Epoch epoch_from_local(int year, int month, int day, int hour, int minute, int second);

// Sets the process timezone. Host tests and the host-platform build call this;
// on the device ESPHome owns TZ and this must not be called.
void tz_set(const char *tz);

// ESPHome's own definition of a trustworthy clock: year >= 2019. Before SNTP or
// an RTC read lands, the clock reads 1970 and nothing scheduled may fire
// (fault-policy row 6). Manual runs are still allowed -- a duration is a
// stopwatch, not a clock.
inline constexpr int16_t MIN_TRUSTED_YEAR = 2019;

}  // namespace irrigation_core

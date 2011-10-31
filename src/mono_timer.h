/* This file is intended to make it easier to work with monotonic timers on
 * multiple platforms, since there's no single API for doing so.
 *
 * On Windows, we use QueryPerformanceCounter and QueryPerformanceFrequency
 * On OSX, we use mach_absolute_time()
 * On Linux, we use clock_gettime() with CLOCK_MONOTONIC
 * Other platforms are currently unsupported.
 *
 * Since I find this problem worth solving somewhat permanently:
 *
 * I hereby release this file into the public domain.
 */
#ifndef __MONO_TIMER_H__
#define __MONO_TIMER_H__

#include <stdint.h>

// Returns the current time in nanoseconds since an arbitrary epoch.  This
// value is supposed to be stable and increase monotonically when called from
// the same process, up to the maximum guarantee that can be provided by the
// underlying OS.
// Returns 0 on error.
uint64_t mono_timer_nanos();

#endif // __MONO_TIMER_H__

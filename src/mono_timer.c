/* This file is intended to make it easier to work with monotonic timers on
 * multiple platforms, since there's no single API for doing so.
 *
 * On Windows, we use QueryPerformanceCounter and QueryPerformanceFrequency
 * On OSX, we use mach_absolute_time()
 * On Linux, we use clock_gettime() with CLOCK_MONOTONIC
 *
 * Since I find this problem worth solving somewhat permanently:
 *
 * I hereby release this file into the public domain.
 */

#include "mono_timer.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

#ifdef __linux__
#include <time.h>
#endif

#ifdef _WIN32
uint64_t mono_timer_nanos() {
	LARGE_INTEGER freq;
	LARGE_INTEGER count;
	if (!QueryPerformanceFrequency(&freq)) return 0;
	if (!QueryPerformanceCounter(&count)) return 0;
	// count / freq = seconds, so count / freq * 1e9 = nanoseconds
	return (uint64_t)(count.QuadPart * 1000000000 / freq.QuadPart);
}
#endif // _WIN32
#ifdef __APPLE__
uint64_t mono_timer_nanos() {
	mach_timebase_info_data_t info;
	mach_timebase_info(&info);
	return mach_absolute_time() * info.numer / info.denom;
}
#endif // APPLE
#ifdef __linux__
uint64_t mono_timer_nanos() {
	struct timespec ts;
	if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
	uint64_t nanos = (uint64_t)ts.tv_sec * 1000000000;
	nanos += ts.tv_nsec;
	return nanos;
}
#endif // __linux__

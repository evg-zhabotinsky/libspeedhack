#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <dlfcn.h>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>
#include <clocale>
#include <cstring>
#include <mutex>

using namespace std;

// Template magic to avoid "conflicting declaration" for `gettimeofday` due to deprecated parameter `tz` type changes
template<typename F> struct TzTypeHelper;
template<typename TV, typename TZ> struct TzTypeHelper<int(TV, TZ)> { using type = TZ; };
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
using TzTypeGet = TzTypeHelper<decltype(gettimeofday)>::type;
using TzTypeSet = TzTypeHelper<decltype(settimeofday)>::type;
#pragma GCC diagnostic pop

static timeval& operator *= (timeval &tv, double x)
{
	double sec = tv.tv_sec * x, usec = tv.tv_usec * x;
	tv.tv_sec = floor(sec);
	usec += 1000000 * (sec - tv.tv_sec);
	tv.tv_usec = fmod(usec, 1000000);
	tv.tv_sec += usec / 1000000;
	return tv;
}

static timeval& operator += (timeval &tv, const timeval &x)
{
	tv.tv_sec += x.tv_sec;
	tv.tv_usec += x.tv_usec;
	if (tv.tv_usec >= 1000000) {
		tv.tv_usec -= 1000000;
		tv.tv_sec += 1;
	}
	return tv;
}

static timeval& operator -= (timeval &tv, const timeval &x)
{
	tv.tv_sec -= x.tv_sec;
	tv.tv_usec -= x.tv_usec;
	if (tv.tv_usec < 0) {
		tv.tv_usec += 1000000;
		tv.tv_sec -= 1;
	}
	return tv;
}

static timeval operator * (const timeval &tv, double x)
{
	timeval res(tv);
	res *= x;
	return res;
}

/* Unused
static timeval operator * (double x, const timeval &tv)
{
	return tv * x;
}
*/

static timeval operator + (const timeval &tv, const timeval &x)
{
	timeval res(tv);
	res += x;
	return res;
}

static timeval operator - (const timeval &tv, const timeval &x)
{
	timeval res(tv);
	res -= x;
	return res;
}

static timespec& operator *= (timespec &tv, double x)
{
	double sec = tv.tv_sec * x, usec = tv.tv_nsec * x;
	tv.tv_sec = floor(sec);
	usec += 1000000000 * (sec - tv.tv_sec);
	tv.tv_nsec = fmod(usec, 1000000000);
	tv.tv_sec += usec / 1000000000;
	return tv;
}

static timespec& operator += (timespec &tv, const timespec &x)
{
	tv.tv_sec += x.tv_sec;
	tv.tv_nsec += x.tv_nsec;
	if (tv.tv_nsec >= 1000000000) {
		tv.tv_nsec -= 1000000000;
		tv.tv_sec += 1;
	}
	return tv;
}

static timespec& operator -= (timespec &tv, const timespec &x)
{
	tv.tv_sec -= x.tv_sec;
	tv.tv_nsec -= x.tv_nsec;
	if (tv.tv_nsec < 0) {
		tv.tv_nsec += 1000000000;
		tv.tv_sec -= 1;
	}
	return tv;
}

static timespec operator * (const timespec &tv, double x)
{
	timespec res(tv);
	res *= x;
	return res;
}

/* Unused
static timespec operator * (double x, const timespec &tv)
{
	return tv * x;
}
*/

static timespec operator + (const timespec &tv, const timespec &x)
{
	timespec res(tv);
	res += x;
	return res;
}

static timespec operator - (const timespec &tv, const timespec &x)
{
	timespec res(tv);
	res -= x;
	return res;
}

static timeval timezero, _timezero;
static timespec
	realtimezero, realtimecoarsezero,
	monotoniczero, monotoniccoarsezero, monotonicrawzero,
	boottimezero,
	_realtimezero, _realtimecoarsezero,
	_monotoniczero, _monotoniccoarsezero, _monotonicrawzero,
	_boottimezero;

static int (*gettimeofday_orig)(timeval *tv, TzTypeGet tz);
static int (*clock_gettime_orig)(clockid_t clk_id, timespec *tp);

static double speedup = 1;
static int fd;
static constexpr int buf_len = 32;
static char str[buf_len];
static int str_len;

FILE *efile;
#define log(...) do {            \
	fprintf(efile, "LibSpeedhack: " __VA_ARGS__); \
	fflush(efile);               \
} while (0)
#ifdef DEBUG
#define dbglog(...) log("Debug: " __VA_ARGS__)
#else
#define dbglog(...) do {} while(0)
#endif

static void fix_timescale() {
	double news;
	{  // Read new timescale value, if any, else return
		if (fd < 0) {
			return;
		}
		if (str_len < 0) {  // Error indicator, flush until newline
			dbglog("flushing after error\n");
			for (;;) {
				int n = read(fd, str, buf_len);
				if (n <= 0) {
					dbglog("flush: no more data or error %d\n", n);
					return;
				}
				dbglog("flush: read %d: %.*s\n", n, n, str);
				char *p = (char*)memchr(str, '\n', n);
				if (p != nullptr) {
					dbglog("flush: found newline\n");
					p++;
					str_len = str + n - p;
					if (str_len > 0) {
						memmove(str, p, str_len);
					}
					dbglog("flush: remainder %d: %.*s\n", str_len, str_len, str);
					n = read(fd, str + str_len, buf_len - str_len);
					if (n < 0 && errno != EAGAIN) {
						dbglog("read error %d: %s\n", errno, strerror(errno));
						str_len = -1;
						return;
					}
					if (n > 0) {
						str_len += n;
					}
					break;
				}
				if (n < buf_len) {
					dbglog("flush: not full buffer and no newline\n");
					return;
				}
			}
		} else {
			// Read more until newline, return if none yet
			int n = read(fd, str + str_len, buf_len - str_len);
#ifdef DEBUG
			static const char ticker[] = "-\\|/";
			static const char* tp = nullptr;
			if (n > 0 || tp == nullptr) {
				dbglog("was %d: %.*s\n", str_len, str_len, str);
			}
#endif
			if (n < 0 && errno != EAGAIN) {
				dbglog("read error %d: %s\n", errno, strerror(errno));
				str_len = -1;
				return;
			}
			if (n <= 0) {
#ifdef DEBUG
				if (tp == nullptr) {
					dbglog("nothing read\n");
					tp = ticker;
				} else {
					dbglog("%c\r", *tp);
					if (*++tp == 0) {
						tp = ticker;
					}
				}
#endif
				return;
			}
			str_len += n;
		}
		dbglog("now %d: %.*s\n", str_len, str_len, str);
		char *end = (char*)memrchr(str, '\n', str_len);
		if (end == nullptr) {
			if (str_len < buf_len) {
				dbglog("no newline yet\n");
				return;
			}
			// Maximum line length reached, parse anyway
			dbglog("max line length\n");
			end = str + str_len - 1;
			str_len = -1;  // Flush the remainder of the line
		} else {
			str_len -= end + 1 - str;
		}
		// Found last line end, isolate the line
		char *start = (char*)memrchr(str, '\n', end - str);
		if (start == nullptr) {
			start = str;
		} else {
			start++;
		}
		*end = 0;
		dbglog("line %d: %s\n", int(end-start), start);
		// Read the first floating point number in that line
		{ // Set locale to "C" to use decimal dot, parse, then restore locale
			const char *lc = setlocale(LC_NUMERIC, "C");
			int r = sscanf(start, "%lf", &news);
			setlocale(LC_NUMERIC, lc);
			if (r < 1) {
				news = -1;  // No float parsed, so setting to invalid
			}
		}
		dbglog("read as %lf\n", news);
		// Move the remainder of the string to the start
		if (str_len > 0) {
			memmove(str, end + 1, str_len);
		}
		// Log read timescale value to log
		dbglog("remainder %d: %.*s\n", str_len, str_len, str);
		if (news <= 0) {
			log("invalid input\n");
			return;
		}
		log("new timescale %lf\n", news);
	}

	// Apply the new timescale
	if (gettimeofday_orig) {
		timeval tv;
		gettimeofday_orig(&tv, nullptr);
		_timezero += (tv - timezero) * speedup;
		timezero = tv;
	}
	if (clock_gettime_orig) {
		timespec tv;
		clock_gettime_orig(CLOCK_REALTIME, &tv);
		_realtimezero += (tv - realtimezero) * speedup;
		realtimezero = tv;
		clock_gettime_orig(CLOCK_REALTIME_COARSE, &tv);
		_realtimecoarsezero += (tv - realtimecoarsezero) * speedup;
		realtimecoarsezero = tv;
		clock_gettime_orig(CLOCK_MONOTONIC, &tv);
		_monotoniczero += (tv - monotoniczero) * speedup;
		monotoniczero = tv;
		clock_gettime_orig(CLOCK_MONOTONIC_COARSE, &tv);
		_monotoniccoarsezero += (tv - monotoniccoarsezero) * speedup;
		monotoniccoarsezero = tv;
		clock_gettime_orig(CLOCK_MONOTONIC_RAW, &tv);
		_monotonicrawzero += (tv - monotonicrawzero) * speedup;
		monotonicrawzero = tv;
		clock_gettime_orig(CLOCK_BOOTTIME, &tv);
		_boottimezero += (tv - boottimezero) * speedup;
		boottimezero = tv;
	}
	speedup = news;
}

static mutex *the_mutex;

static void init_libspeedhack()
{
	if (the_mutex != nullptr) {
		return;
	}
	the_mutex = new mutex;
	gettimeofday_orig = decltype(gettimeofday_orig)(dlsym(RTLD_NEXT,"gettimeofday"));
	clock_gettime_orig = decltype(clock_gettime_orig)(dlsym(RTLD_NEXT,"clock_gettime"));
	efile = fopen("/tmp/speedhack_log", "a");
	fd = open("/tmp/speedhack_pipe", O_RDONLY | O_NONBLOCK);
	fix_timescale();
}

// Constructor of this runs somewhere during process init
static struct Init {
	Init() {
		init_libspeedhack();
	}
} init_struct;

// Put at the start of every externally called function that messes with time
#define LOCKED_TS                        \
	if (the_mutex == nullptr) {          \
		init_libspeedhack();             \
	}                                    \
	lock_guard<mutex> _lock(*the_mutex); \
	fix_timescale();

extern "C" int gettimeofday(timeval *tv, TzTypeGet tz)
{
	LOCKED_TS
	int val = gettimeofday_orig(tv, tz);
	*tv = _timezero + (*tv - timezero) * speedup;
	return val;
}

extern "C" int clock_gettime(clockid_t clk_id, timespec *tp)
{
	LOCKED_TS
	timespec z, _z;
	switch (clk_id) {
	case CLOCK_REALTIME:
		z = realtimezero;
		_z = _realtimezero;
		break;
	case CLOCK_REALTIME_COARSE:
		z = realtimecoarsezero;
		_z = _realtimecoarsezero;
		break;
	case CLOCK_MONOTONIC:
		z = monotoniczero;
		_z = _monotoniczero;
		break;
	case CLOCK_MONOTONIC_COARSE:
		z = monotoniccoarsezero;
		_z = _monotoniccoarsezero;
		break;
	case CLOCK_MONOTONIC_RAW:
		z = monotonicrawzero;
		_z = _monotonicrawzero;
		break;
	case CLOCK_BOOTTIME:
		z = boottimezero;
		_z = _boottimezero;
		break;
	case CLOCK_PROCESS_CPUTIME_ID:
	case CLOCK_THREAD_CPUTIME_ID:
		return clock_gettime_orig(clk_id, tp);
	default:
		{
			static bool f = true;
			if (f) {
				log("clock_gettime bad clk_id %d\n", clk_id);
				f = false;
			}
		}
		return clock_gettime_orig(clk_id, tp);
	}
	int val = clock_gettime_orig(clk_id, tp);
	*tp = _z + (*tp - z) * speedup;
	return val;
}

extern "C" time_t time(time_t *tloc)
{
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	if (tloc) {
		*tloc = tv.tv_sec;
	}
	return tv.tv_sec;
}

extern "C" int settimeofday(const timeval *tv, TzTypeSet tz)
{
	static bool f = true;
	if (f) {
		log("settimeofday called\n");
		f = false;
	}
	return 0;
}

extern "C" int clock_settime(clockid_t clk_id, const timespec *tp)
{
	static bool f = true;
	if (f) {
		log("clock_settime called\n");
		f = false;
	}
	return 0;
}

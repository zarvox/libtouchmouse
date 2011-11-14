#ifndef __TOUCHMOUSE_INTERNAL__
#define __TOUCHMOUSE_INTERNAL__

#include <libtouchmouse/libtouchmouse.h>
#include <stdarg.h>

struct touchmouse_device_ {
	// HIDAPI handle
	hid_device* dev;
	// Callback information
	void* userdata;
	touchmouse_image_callback cb;
	// Image decoder/reassembler state
	uint8_t timestamp_last_completed;
	uint8_t timestamp_in_progress;
	int buf_index;
	int next_is_run_encoded;
	uint8_t partial_image[181];
	uint8_t image[195];
};

enum {
	DECODER_BEGIN,
	DECODER_IN_PROGRESS,
	DECODER_COMPLETE,
	DECODER_ERROR,
} decoder_state;

void tm_log(touchmouse_loglevel level, const char *fmt, ...);

#define TM_LOG(level, ...) tm_log(level, __VA_ARGS__)

#define TM_FATAL(...) TM_LOG(TOUCHMOUSE_LOG_FATAL, __VA_ARGS__)
#define TM_ERROR(...) TM_LOG(TOUCHMOUSE_LOG_ERROR, __VA_ARGS__)
#define TM_WARNING(...) TM_LOG(TOUCHMOUSE_LOG_WARNING, __VA_ARGS__)
#define TM_NOTICE(...) TM_LOG(TOUCHMOUSE_LOG_NOTICE, __VA_ARGS__)
#define TM_INFO(...) TM_LOG(TOUCHMOUSE_LOG_INFO, __VA_ARGS__)
#define TM_DEBUG(...) TM_LOG(TOUCHMOUSE_LOG_DEBUG, __VA_ARGS__)
#define TM_SPEW(...) TM_LOG(TOUCHMOUSE_LOG_SPEW, __VA_ARGS__)
#define TM_FLOOD(...) TM_LOG(TOUCHMOUSE_LOG_FLOOD, __VA_ARGS__)

#endif // __TOUCHMOUSE_INTERNAL__

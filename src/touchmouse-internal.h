#ifndef __TOUCHMOUSE_INTERNAL__
#define __TOUCHMOUSE_INTERNAL__

#include <libtouchmouse/libtouchmouse.h>

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

#endif // __TOUCHMOUSE_INTERNAL__

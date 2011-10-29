#ifndef __LIBTOUCHMOUSE_H__
#define __LIBTOUCHMOUSE_H__
#include <stdint.h>

// Types:
struct touchmouse_device_;
typedef struct touchmouse_device_ touchmouse_device;

struct touchmouse_device_info; // For enumeration.
typedef struct touchmouse_device_info {
	struct touchmouse_device_info *next;
	void* opaque; // Internally-used unique handle for the device in question
	// We'll add any other interesting info, like serial number, version ID, etc. if we can fetch it reliably.
} touchmouse_device_info;


typedef struct touchmouse_callback_info {
	void* userdata;
	uint8_t* image;
	uint8_t timestamp;
} touchmouse_callback_info;

// Callback declaration: void function that takes a void pointer,
typedef void (*touchmouse_image_callback)(touchmouse_callback_info *cbinfo);

typedef enum {
	TOUCHMOUSE_DEFAULT = 0, // Default mode when you plug the mouse in.
	TOUCHMOUSE_RAW_IMAGE = 1, // Disables gestures, calls touchmouse_image_callback whenever a new image arrives
	// Other modes may exist, I haven't played with the mouse enough yet.
} touchmouse_mode;

// Library initialization/destruction
int touchmouse_init(void);
int touchmouse_shutdown(void);

// Device enumeration/open/close/free
touchmouse_device_info* touchmouse_enumerate_devices(void);
void touchmouse_free_enumeration(touchmouse_device_info *devs);
int touchmouse_open(touchmouse_device **dev, touchmouse_device_info *dev_info);
int touchmouse_close(touchmouse_device *dev);

// Set mouse mode.
int touchmouse_set_device_mode(touchmouse_device *dev, touchmouse_mode mode);

// Register callback for touch image updates
int touchmouse_set_image_update_callback(touchmouse_device *dev, touchmouse_image_callback callback);

// Allow for setting a piece of user-defined data to be provided in the callback.
int touchmouse_set_device_userdata(touchmouse_device *dev, void *userdata);

// Process events for a device.
// milliseconds < 0 means "block until you have new data and trigger the callback, then return."
// milliseconds = 0 means "trigger the callback if you have the data waiting, otherwise request data async and return immediately"
// milliseconds > 0 means "fetch data.  Trigger a callback if the data arrives within <arg> milliseconds, otherwise return."
int touchmouse_process_events_timeout(touchmouse_device *dev, int milliseconds);

#endif /* __LIBTOUCHMOUSE_H__ */
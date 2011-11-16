#ifndef __LIBTOUCHMOUSE_H__
#define __LIBTOUCHMOUSE_H__
#include <stdint.h>

// Win32 needs symbols exported.
#ifndef _WIN32
	#define TOUCHMOUSEAPI
#else
	#ifdef __cplusplus
		#define TOUCHMOUSEAPI extern "C" __declspec(dllexport)
	#else
		#define TOUCHMOUSEAPI __declspec(dllexport)
	#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct touchmouse_device_;
/// Opaque struct representing a handle to a particular TouchMouse device.
typedef struct touchmouse_device_ touchmouse_device;

struct touchmouse_device_info;
/// Struct used for enumeration of devices.
typedef struct touchmouse_device_info {
	struct touchmouse_device_info *next; /**< Pointer to next touchmouse_device_info in the linked list (NULL if this is the last item in the list) */
	void* opaque;                        /**< Internally-used unique handle for the device in question */
	// We'll add any other interesting info, like serial number, version ID, etc. if we can fetch it reliably.
} touchmouse_device_info;

/// Information provided in image update callbacks
typedef struct touchmouse_callback_info {
	void* userdata;    /**< User-controllable pointer to allow determination of higher-level context */
	uint8_t* image;    /**< Pointer to 195 bytes of 8-bit greyscale image data (13 rows, 15 columns). */
	uint8_t timestamp; /**< Device-provided timestamp associated with this image.  Differences in timestamps can be interpreted as differences in milliseconds, up to 255 ms. */
} touchmouse_callback_info;

/// Callback declaration: void function that takes a pointer to a touchmouse_callback_info
typedef void (*touchmouse_image_callback)(touchmouse_callback_info *cbinfo);

/// A list of modes that the touchmouse can be placed in.
typedef enum {
	TOUCHMOUSE_DEFAULT = 0,   /**< Default mode when you plug the mouse in, no full image callbacks. */
	TOUCHMOUSE_RAW_IMAGE = 1, /**< Disables gestures, calls touchmouse_image_callback whenever a new image arrives */
	// Other modes may exist, I haven't played with the mouse enough yet.
} touchmouse_mode;

/// Enumeration of library message logging levels
typedef enum {
	TOUCHMOUSE_LOG_FATAL = 0, /**< Log level for crashing/non-recoverable errors */
	TOUCHMOUSE_LOG_ERROR,     /**< Log level for major errors */
	TOUCHMOUSE_LOG_WARNING,   /**< Log level for warning messages */
	TOUCHMOUSE_LOG_NOTICE,    /**< Log level for important messages */
	TOUCHMOUSE_LOG_INFO,      /**< Log level for normal messages */
	TOUCHMOUSE_LOG_DEBUG,     /**< Log level for useful development messages */
	TOUCHMOUSE_LOG_SPEW,      /**< Log level for slightly less useful messages */
	TOUCHMOUSE_LOG_FLOOD,     /**< Log level for EVERYTHING. May significantly slow performance. */
} touchmouse_loglevel;

// Library initialization/destruction routines

/**
 * Initialize libtouchmouse.
 *
 * This function must be called before using any other libtouchmouse function.
 *
 * @return 0 on success, < 0 on error
 */
TOUCHMOUSEAPI int touchmouse_init(void);

/**
 * Deinitialize libtouchmouse and free related internal data structures.
 *
 * @return 0 on success, < 0 on error
 */
TOUCHMOUSEAPI int touchmouse_shutdown(void);

/**
 * Set global logging verbosity for the touchmouse library to the provided
 * loglevel.
 *
 * Since we currently lack contexts (since the underlying HIDAPI lacks
 * contexts) we can only set this globally.
 *
 * @param level Maximum verbosity level to print statements at (see touchmouse_loglevel for details).
 */
TOUCHMOUSEAPI void touchmouse_set_log_level(touchmouse_loglevel level);

/**
 * Enumerate all currently-connected TouchMouse devices.
 *
 * @return A linked list of touchmouse_device_info structs, or NULL if no devices are present.
 */
TOUCHMOUSEAPI touchmouse_device_info* touchmouse_enumerate_devices(void);

/**
 * Free a linked list of touchmouse_device_info returned from
 * touchmouse_enumerate_devices.
 *
 * @param devs The original value returned from touchmouse_enumerate_devices to free.
 */
TOUCHMOUSEAPI void touchmouse_free_enumeration(touchmouse_device_info *devs);

/**
 * Open a previously-enumerated (but not yet freed!) touchmouse_device_info.
 *
 * @param dev Address of a touchmouse_device* to populate with a handle to the opened device.
 * @param dev_info Device information struct describing the device to open
 *
 * @return 0 on success, < 0 on error.  On success, *dev will be populated with a handle to the opened device.
 */
TOUCHMOUSEAPI int touchmouse_open(touchmouse_device **dev, touchmouse_device_info *dev_info);

/**
 * Close the device referred to by the provided handle.
 *
 * Closing the device leaves it in whatever mode it was last placed in.
 *
 * @param dev Device handle to close
 * @return 0 on success, < 0 on error
 */
TOUCHMOUSEAPI int touchmouse_close(touchmouse_device *dev);

/**
 * Put the selected device in the selected mode (ie, full image updates or
 * hardware-supported gestures).
 *
 * @param dev Device for which to set the mode
 * @param mode Desired mode to put the device in.
 *
 * @return 0 on success, < 0 on error
 */
TOUCHMOUSEAPI int touchmouse_set_device_mode(touchmouse_device *dev, touchmouse_mode mode);

/**
 * Register a callback to be called when a touch image update is received from
 * the device.
 *
 * @param dev Device for which to set the image update callback function
 * @param callback Function to be called when an image update is received
 *
 * @return 0 on success, < 0 on error
 */
TOUCHMOUSEAPI int touchmouse_set_image_update_callback(touchmouse_device *dev, touchmouse_image_callback callback);

/**
 * Set a piece of user-defined data to be provided in the callback.  This makes
 * it possible to distinguish higher-level data associated with a particular
 * static callback.
 *
 * @param dev Device handle to set a particular userdata pointer.
 * @param userdata Pointer to desired userdata value to receive in the touchmouse_callback_info in callbacks for this device.
 *
 * @return 0 on success, < 0 on error
 */
TOUCHMOUSEAPI int touchmouse_set_device_userdata(touchmouse_device *dev, void *userdata);

/**
 * Process events for a device for up to a certain maximium of milliseconds.
 *
 *     - milliseconds < 0 means "block until you have new data and trigger the callback, then return."
 *     - milliseconds = 0 means "trigger the callback if you have the requisite data waiting, otherwise request data asynchronously and return immediately"
 *     - milliseconds > 0 means "fetch data.  Trigger a callback if the data arrives within <arg> milliseconds, otherwise return."
 *
 * @param dev Device for which to process events.
 * @param milliseconds Maximum time to block waiting for a new image update.  Negative values mean block infinitely, 0 means completely nonblocking, and positive values set a maximum timeout.
 *
 * @return 0 on success, -1 on temporary error, -2 on unrecoverable error
 */
TOUCHMOUSEAPI int touchmouse_process_events_timeout(touchmouse_device *dev, int milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* __LIBTOUCHMOUSE_H__ */

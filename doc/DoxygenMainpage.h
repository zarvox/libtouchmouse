/**
@mainpage libtouchmouse
@author Drew Fisher (drew.m.fisher@gmail.com)

An open-source, cross-platform driver for the Microsoft TouchMouse

Sources: http://github.com/zarvox/libtouchmouse

@section libtouchmouseIntro Introduction

libtouchmouse is a userspace driver for the Microsoft TouchMouse built on HIDAPI
(http://www.signal11.us/oss/hidapi/).  While the TouchMouse will work as a
basic mouse on all platforms that implement USB mice through HID, this driver
is interesting in that it provides access to the device's raw touch image matrix.

@section libtouchmouseUsage Usage

libtouchmouse is properly used in the following manner:

- Initialize the library with touchmouse_init()
- Enumerate devices with touchmouse_enumerate_devices()
- Open a device (or multiple devices) with touchmouse_open()
- Free the device enumeration with touchmouse_free_enumeration()
- Put the device in full image updates mode with touchmouse_set_device_mode()
- Set a callback for the device with touchmouse_set_image_update_callback()
- Repeatedly call touchmouse_process_events_timeout(), which will call your callback function to get image updates.  You may wish to do this in a blocking manner in a thread.
- When done with the device, (optionally) set the device back in default mode with touchmouse_set_device_mode() and close the device with touchmouse_close()
- Deinitialize the library with touchmouse_shutdown()

See the examples for usage examples that you're free to copy and modify at will.

*/

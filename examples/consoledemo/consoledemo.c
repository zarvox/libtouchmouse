/*
 * Copyright 2011 Drew Fisher (drew.m.fisher@gmail.com).
 *
 * The contents of this file may be used by anyone for any reason without any
 * conditions and may be used as a starting point for your own applications
 * which use libtouchmouse.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <libtouchmouse/libtouchmouse.h>

void callback(touchmouse_callback_info *cbdata) {
	printf("Callback triggered, timestamp %d\n", cbdata->timestamp);
	int row;
	for(row = 0; row < 13 ; row++) {
		int col;
		for(col = 0; col < 15; col++) {
			printf("%02X ", cbdata->image[row * 15 + col]);
		}
		printf("\n");
	}
}

int main(void) {
	int res;
	// Initialize library.
	printf("Initializing libtouchmouse...\n");
	res = touchmouse_init();
	if (res != 0) {
		fprintf(stderr, "Failed to initialize libtouchmouse, aborting\n");
		return 1;
	}

	// Count devices.
	printf("Enumerating touchmouse devices...\n");
	int devs_found = 0;
	touchmouse_device_info* devs = touchmouse_enumerate_devices();
	printf("touchmouse_enumerate_devices returned %p\n", devs);
	touchmouse_device_info* d = devs;
	while(d) {
		d = d->next;
		devs_found++;
	}
	printf("Found %d touchmouse devices.\n", devs_found);

	// If we didn't see any, we can't possibly open a device.
	if (devs_found == 0) {
		fprintf(stderr, "No touchmouse found, aborting\n");
		return 1;
	}

	// Open the first device.
	printf("Attempting to open device 0...\n");
	touchmouse_device *dev;
	res = touchmouse_open(&dev, devs);
	touchmouse_free_enumeration(devs);
	if (res != 0) {
		fprintf(stderr, "Failed to open device 0, aborting\n");
		return -1;
	}
	printf("Opened device 0 successfully.\n");

	// Enable full image updates on the opened device
	printf("Setting device to send full image updates...\n");
	res = touchmouse_set_device_mode(dev, TOUCHMOUSE_RAW_IMAGE);
	if (res != 0) {
		fprintf(stderr, "Failed to enable full image updates, aborting\n");
		return -1;
	}
	printf("Device set to send full image updates successfully.\n");

	// Set user callback function
	printf("Setting callback function for image updates...\n");
	res = touchmouse_set_image_update_callback(dev, callback);
	if (res != 0) {
		fprintf(stderr, "Failed to set callback function, aborting\n");
		return -1;
	}

	// Poll device for image updates.
	int i = 0;
	while(i < 100) {
		res = touchmouse_process_events_timeout(dev, -1); // -1 means infinite timeout
		i++;
	}

	// Disable touch image updates (and reenable automatic two-finger scrolling, etc.)
	printf("Restoring device to default mode...\n");
	res = touchmouse_set_device_mode(dev, TOUCHMOUSE_DEFAULT);
	if (res != 0) {
		fprintf(stderr, "Failed to set device back to default mode, aborting\n");
		return -1;
	}

	// Close device handle
	printf("Closing device...\n");
	res = touchmouse_close(dev);
	dev = NULL;
	if (res != 0) {
		fprintf(stderr, "Failed to close device, aborting\n");
		return -1;
	}

	// Free static library data
	printf("Cleaning up libtouchmouse...\n");
	touchmouse_shutdown();
	if (res != 0) {
		fprintf(stderr, "Failed to clean up libtouchmouse, aborting\n");
		return -1;
	}
	printf("Done!\n");
	return 0;
}

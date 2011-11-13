/* Copyright 2011 Drew Fisher (drew.m.fisher@gmail.com). All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ''AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of the copyright holder.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hidapi.h"
#include <stdint.h>

#include "touchmouse-internal.h"
#include "mono_timer.h"

#pragma pack(1)
//  The USB HID reports that contain our data are always 32 bytes, with the
//  following structure:
typedef struct {
	uint8_t report_id; // HID report ID.  In this case, we only care about the
	                   //   ones that have report_id 0x27.
	uint8_t length;    // Length of the useful data in this transfer, including
	                   //   both timestamp and data[] buffer.
	uint8_t magic[4];  // Four magic bytes.  These are always the same:
	                   //   0x14 0x01 0x00 0x51
	uint8_t timestamp; // Measured in milliseconds since the last series of
	                   //   touch events began, but wraps at 256.  If two or
	                   //   more consecutive transfers have the same timestamp,
	                   //   it is likely that their data should be taken
	                   //   together 
	uint8_t data[25];  // Compressed touchmouse image data.  Only length-1
	                   //   bytes of this buffer are useful.
} report;
#pragma pack()

//  The image that we get is 181 bytes that are part of a 13x15 (195 pixel)
//  grid laid out like this:
//
//  1 2 3 4 5 6 7 8 9 a b c d e f
//  -----------------------------+
//        0 0 0 0 0 0 0 0 0      | 1
//      0 0 0 0 0 0 0 0 0 0 0    | 2
//    0 0 0 0 0 0 0 0 0 0 0 0 0  | 3
//    0 0 0 0 0 0 0 0 0 0 0 0 0  | 4
//  0 0 0 0 0 0 0 0 0 0 0 0 0 0 0| 5
//  0 0 0 0 0 0 0 0 0 0 0 0 0 0 0| 6
//  0 0 0 0 0 0 0 0 0 0 0 0 0 0 0| 7
//  0 0 0 0 0 0 0 0 0 0 0 0 0 0 0| 8
//  0 0 0 0 0 0 0 0 0 0 0 0 0 0 0| 9
//  0 0 0 0 0 0 0 0 0 0 0 0 0 0 0| 10
//  0 0 0 0 0 0 0 0 0 0 0 0 0 0 0| 11
//  0 0 0 0 0 0 0 0 0 0 0 0 0 0 0| 12
//  0 0 0 0 0 0 0 0 0 0 0 0 0 0 0| 13
//
//  Each "pixel" in the image may have one of 15 values - 0 through 0xE.  We
//  receive a stream of nybbles that encodes these 181 bytes, but should present
//  the unpacked version to the client.
//
//  The data we receive is ordered as is English - left to right, top to bottom.
//
//  When processing nybbles, it turns out we should process the less-significant
//  nybble first and the more-significant nybble second.
//
//  The compression scheme is simple - pixels are expressed with their raw value
//  (0 - E) unless there is a run of at least three 0's, in which case two nybbles
//  F, X encode a run of X+3 zeroes.  Since zeroes are the most common value, this
//  results in a decent amount of compression.
//
//  Note that the usable touch area on the mouse may be even smaller than this 181
//  pixel arrangement - some of even these pixels may always give a value of 0.

static void reset_decoder(touchmouse_device *state)
{
	state->buf_index = 0;
	state->next_is_run_encoded = 0;
	memset(state->partial_image, 0, sizeof(state->partial_image));
	memset(state->image, 0, sizeof(state->image));
}

// There are 15 possible values that each pixel can take on, but we'd like to
// scale them up to the full range of a uint8_t for convenience.
static uint8_t decoder_table[15] = {0, 18, 36, 55, 73, 91, 109, 128, 146, 164, 182, 200, 219, 237, 255 };

static int process_nybble(touchmouse_device *state, uint8_t nybble)
{
	//printf("process_nybble: buf_index = %d\t%01x\n", state->buf_index, nybble);
	if (nybble >= 16) {
		fprintf(stderr, "process_nybble: got nybble >= 16, wtf: %d\n", nybble);
		return DECODER_ERROR;
	}
	if (state->next_is_run_encoded) {
		// Previous nybble was 0xF, so this one is (the number of bytes to skip - 3)
		if (state->buf_index + nybble + 3 > 181) {
			// Completing this decode would overrun the buffer.  We've been
			// given invalid data.  Abort.
			fprintf(stderr, "process_nybble: run encoded would overflow buffer: got 0xF%X (%d zeros) with only %d bytes to fill in buffer\n", nybble, nybble + 3, 181 - state->buf_index);
			return DECODER_ERROR;
		}
		int i;
		for(i = 0 ; i < nybble + 3; i++) {
			state->partial_image[state->buf_index] = 0;
			state->buf_index++;
		}
		state->next_is_run_encoded = 0;
	} else {
		if (nybble == 0xf) {
			state->next_is_run_encoded = 1;
		} else {
			state->partial_image[state->buf_index] = nybble;
			state->buf_index++;
		}
	}
	// If we're done collecting the data, unpack it into image as described above
	// This could probably be reworked to unpack the image in-place reusing the
	// image buffer, but right now I'm being lazy.
	if (state->buf_index == 181) {
		memset(state->image, 0, 195);
		int row;
		int i = 0;
		int startcol;
		int endcol;
		for(row = 0; row < 13 ; row++) {
			switch(row) {
				// Note: inclusive bounds
				case 0: startcol = 0x3;
						endcol = 0xb;
						break;
				case 1: startcol = 0x2;
						endcol = 0xc;
						break;
				case 2:
				case 3: startcol = 0x1;
						endcol = 0xd;
						break;
				default:
						startcol = 0x0;
						endcol = 0xe;
						break;
			}
			int col;
			for(col = startcol ; col <= endcol ; col++) {
				state->image[row * 15 + col] = decoder_table[state->partial_image[i++]];
			}
		}
		return DECODER_COMPLETE;
	}
	return DECODER_IN_PROGRESS;
}

// Initialize libtouchmouse.  Which mostly consists of calling hid_init();
int touchmouse_init(void)
{
	return hid_init();
}

// Same thing - clean up
int touchmouse_shutdown(void)
{
	// TODO: add some checking to see if all device handles have been closed,
	// and try to close them all?  This would involve keeping a list of
	// currently-open devices.  Not hard.
	return hid_exit();
}

// Enumeration.
touchmouse_device_info* touchmouse_enumerate_devices(void)
{
	touchmouse_device_info* retval = NULL;
	touchmouse_device_info** prev_next_pointer = &retval;
	struct hid_device_info *devs, *cur_dev;
	// Get list of HID devices that match VendorID/ProductID
	devs = hid_enumerate(0x045e, 0x0773); // 0x045e = Microsoft, 0x0773 = TouchMouse
	cur_dev = devs;
	while(cur_dev) {
		printf("Examining device: %s\n", cur_dev->path);
		printf("\tVendor  ID: %04x\n", cur_dev->vendor_id);
		printf("\tProduct ID: %04x\n", cur_dev->product_id);
		printf("\tSerial num: %ls\n", cur_dev->serial_number);
		printf("\tRelease #:  %d\n", cur_dev->release_number);
		printf("\tManuf. Str: %ls\n", cur_dev->manufacturer_string);
		printf("\tProd.  Str: %ls\n", cur_dev->product_string);
		printf("\tUsage Page: %02x\n", cur_dev->usage_page);
		printf("\tUsage     : %02x\n", cur_dev->usage);
		printf("\tInterface:  %d\n", cur_dev->interface_number);
#ifdef __APPLE__
		// This method of detection should work on both OSX and Windows.
		if (cur_dev->usage_page == 0x0c && cur_dev->usage == 0x01)
#else
		// This method of detection should work on Linux and Windows.
		// Pity there's no single way to uniquely ID the device.
		if (cur_dev->interface_number == 2)
#endif
		{
			printf("Found TouchMouse: %s\n", cur_dev->path);
			*prev_next_pointer = (touchmouse_device_info*)malloc(sizeof(touchmouse_device_info));
			memset(*prev_next_pointer, 0, sizeof(**prev_next_pointer));
			printf("Allocated a touchmouse_device_info at address %p\n", *prev_next_pointer);
			// We need to save both the pointer to this particular hid_device_info
			// as well as the one from which it was initially allocated, so we can
			// free it.
			// Perhaps this would be better placed in a statically allocated list...
			struct hid_device_info** pair = (struct hid_device_info**)malloc(2*sizeof(struct hid_device_info*));
			printf("Allocated two hid_device_info* at address %p\n", pair);
			(*prev_next_pointer)->opaque = (void*)pair;
			pair[0] = cur_dev;
			pair[1] = devs;
			prev_next_pointer = &((*prev_next_pointer)->next);
		}
		cur_dev = cur_dev->next;
	}
	// If we're about to return NULL, then we'd better free the HID enumeration
	// handles now, since we'll get no data from the user when they call
	// touchmouse_free_enumeration(NULL)
	if (!retval) {
		printf("Found no devices, so calling hid_free_enumeration()\n");
		hid_free_enumeration(devs);
	}
	return retval;
}

void touchmouse_free_enumeration(touchmouse_device_info *devs)
{
	touchmouse_device_info* prev;
	if (devs) {
		hid_free_enumeration(((struct hid_device_info**)devs->opaque)[1]);
	}
	while (devs) {
		prev = devs;
		devs = devs->next;
		free(prev->opaque);
		free(prev);
	}
}


int touchmouse_open(touchmouse_device **dev, touchmouse_device_info *dev_info)
{
	touchmouse_device* t_dev = (touchmouse_device*)malloc(sizeof(touchmouse_device));
	memset(t_dev, 0, sizeof(touchmouse_device));
	char* path = ((struct hid_device_info**)dev_info->opaque)[0]->path;
	t_dev->dev = hid_open_path(path);
	if (!t_dev->dev) {
		fprintf(stderr, "hid_open() failed for device with path %s\n", path);
		free(t_dev);
		return -1;
	}
	hid_set_nonblocking(t_dev->dev, 1); // Enable nonblocking reads
	*dev = t_dev;
	return 0;
}

int touchmouse_close(touchmouse_device *dev)
{
	hid_close(dev->dev);
	free(dev);
	return 0;
}

int touchmouse_set_device_mode(touchmouse_device *dev, touchmouse_mode mode)
{
	// We need to set two bits in a particular Feature report.  We first fetch
	// the current state of the feature report, set the interesting bits, and
	// write that feature report back to the device.
	printf("Reading current config flags\n");
	unsigned char data[27] = {0x22};
	int transferred = 0;
	transferred = hid_get_feature_report(dev->dev, data, 27);
	if (transferred > 0) {
		printf("%d bytes received:\n", transferred);
		int i;
		for(i = 0; i < transferred; i++) {
			printf("%02X ", data[i]);
		}
		printf("\n");
	}
	if (transferred != 0x1B) {
		fprintf(stderr, "Failed to read Feature 0x22 correctly; expected 27 bytes, got %d\n", transferred);
		return -1;
	}

	// This particular byte/setting appears to control the
	// "send all the raw input" flag.
	switch (mode) {
		case TOUCHMOUSE_DEFAULT:
			data[4] = 0x00;
			printf("Trying to disable full touch updates...\n");
			break;
		case TOUCHMOUSE_RAW_IMAGE:
			data[4] = 0x06;
			printf("Trying to enable full touch updates...\n");
			break;
	}

	transferred = hid_send_feature_report(dev->dev, data, 27);
	printf("Wrote %d bytes\n", transferred);
	if (transferred == 0x1B) {
		printf("Successfully set device mode.\n");
		return 0;
	}
	fprintf(stderr, "Failed to set device mode.\n");
	return -1;
}

int touchmouse_set_image_update_callback(touchmouse_device *dev, touchmouse_image_callback callback)
{
	dev->cb = callback;
	return 0;
}

int touchmouse_set_device_userdata(touchmouse_device *dev, void *userdata)
{
	dev->userdata = userdata;
	return 0;
}

int touchmouse_process_events_timeout(touchmouse_device *dev, int milliseconds) {
	unsigned char data[256] = {};
	int res;
	uint64_t deadline;
	if(milliseconds == -1) {
		deadline = (uint64_t)(-1);
	} else {
		deadline = mono_timer_nanos() + (milliseconds * 1000000);
	}
	uint64_t nanos = mono_timer_nanos();
	if (nanos == 0 || deadline == 0) {
		fprintf(stderr, "timer function returned an error, erroring out since we have no timer\n");
		return -1;
	}
	do {
		res = hid_read_timeout(dev->dev, data, 255, (deadline - nanos) / 1000000 );
		if (res < 0 ) {
			fprintf(stderr, "hid_read() failed: %d\n", res);
			return -2;
		} else if (res > 0) {
			// Dump contents of transfer
			//printf("Got reply: %d bytes:", res);
			//int j;
			//for(j = 0; j < res; j++) {
			//	printf(" %02X", data[j]);
			//}
			//printf("\n");
			// Interpret contents.
			report* r = (report*)data;
			// We only care about report ID 39 (0x27), which should be 32 bytes long
			if (res == 32 && r->report_id == 0x27) {
				//printf("Timestamp: %02X\t%02X bytes:", r->timestamp, r->length - 1);
				int t;
				//for(t = 0; t < r->length - 1; t++) {
				//	printf(" %02X", r->data[t]);
				//}
				//printf("\n");
				// Reset the decoder if we've seen one timestamp already from earlier
				// transfers, and this one doesn't match.
				if (dev->buf_index != 0 && r->timestamp != dev->timestamp_in_progress) {
					reset_decoder(dev); // Reset decoder for next transfer
				}
				dev->timestamp_in_progress = r->timestamp;
				for(t = 0; t < r->length - 1; t++) { // We subtract one byte because the length includes the timestamp byte.
					int res;
					// Yes, we process the low nybble first.  Embedded systems are funny like that.
					res = process_nybble(dev, r->data[t] & 0xf);
					if (res == DECODER_COMPLETE) {
						dev->timestamp_last_completed = r->timestamp;
						touchmouse_callback_info cbinfo;
						cbinfo.userdata = dev->userdata;
						cbinfo.image = dev->image;
						cbinfo.timestamp = dev->timestamp_last_completed;
						dev->cb(&cbinfo);
						reset_decoder(dev); // Reset decoder for next transfer
						return 0;
					}
					if (res == DECODER_ERROR) {
						fprintf(stderr, "Caught error in decoder, aborting decode!\n");
						reset_decoder(dev);
						return -1;
					}
					res = process_nybble(dev, (r->data[t] & 0xf0) >> 4);
					if (res == DECODER_COMPLETE) {
						dev->timestamp_last_completed = r->timestamp;
						touchmouse_callback_info cbinfo;
						cbinfo.userdata = dev->userdata;
						cbinfo.image = dev->image;
						cbinfo.timestamp = dev->timestamp_last_completed;
						dev->cb(&cbinfo);
						reset_decoder(dev); // Reset decoder for next transfer
						return 0;
					}
					if (res == DECODER_ERROR) {
						fprintf(stderr, "Caught error in decoder, aborting decode!\n");
						reset_decoder(dev);
						return -1;
					}
				}
			}
		}
		nanos = mono_timer_nanos();
	} while(nanos < deadline);
	return 0;
}


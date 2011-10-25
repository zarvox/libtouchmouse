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
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "hidapi.h"
#include <stdint.h>

int quit = 0;

// A signal handler so we can ^C cleanly
void handler(int signum) {
	printf("Caught signal, quitting\n");
	quit = 1;
}

#pragma pack(1)
//  The USB HID reports that contain our data are always 32 bytes, with the
//  following structure:
typedef struct {
	uint8_t report_id; // HID report ID.  In this case, we only care about the
	                   //   ones that have report_id 0x27.
	uint8_t length;    // Length of the useful data in this transfer, including
	                   //   both timestamp and data[] buffer.
	uint32_t magic;    // Four magic bytes.  These are always the same:
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

typedef void (*touchmouse_callback)(void* dev, uint8_t* image, uint8_t timestamp);

// Tracks internal state of the decoder
typedef struct {
	int buf_index;
	int next_is_run_encoded;
	uint8_t partial_image[181];
	uint8_t image[195];
	touchmouse_callback cb;
} decoder;

void print_table(void* placeholder, uint8_t* image, uint8_t timestamp) {
	// Sample callback - simply print out the table.
	int row;
	printf("Current touch state:\n");
	for(row = 0; row < 13 ; row++) {
		int col;
		for(col = 0; col < 15; col++) {
			printf("%02X ", image[row * 15 + col]);
		}
		printf("\n");
	}
}

void reset_decoder(decoder* state) {
	// Don't set the callback to NULL.
	state->buf_index = 0;
	state->next_is_run_encoded = 0;
	memset(state->partial_image, 0, 181);
	memset(state->image, 0, 195);
}

// There are 15 possible values that each pixel can take on, but we'd like to
// scale them up to the full range of a uint8_t for convenience.
uint8_t decoder_table[15] = {0, 18, 36, 55, 73, 91, 109, 128, 146, 164, 182, 200, 219, 237, 255 };

enum {
	DECODER_BEGIN,
	DECODER_IN_PROGRESS,
	DECODER_COMPLETE,
	DECODER_ERROR,
} decoder_state;

int process_nybble(decoder* state, uint8_t nybble) {
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
			fprintf(stderr, "process_nybble: run encoded would overflow buffer: got 0xF%x (%d zeros) with only %d bytes to fill in buffer\n", nybble, nybble + 3, 181 - state->buf_index);
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

int enable_mouse_image_mode(hid_device* dev) {
	// We need to set two bits in a particular Feature report.  We first fetch
	// the current state of the feature report, set the interesting bits, and
	// write that feature report back to the device.
	printf("Reading current config flags\n");
	unsigned char data[27] = {0x22};
	int transferred = 0;
	transferred = hid_get_feature_report(dev, data, 27);
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
	data[4] = 0x06;

	printf("Trying to enable full touch updates...\n");
	transferred = hid_send_feature_report(dev, data, 27);
	printf("Wrote %d bytes\n", transferred);
	if (transferred == 0x1B) {
		printf("Successfully enabled full touch updates.\n");
		return 0;
	}
	fprintf(stderr, "Failed to enable full touch updates.\n");
	return -1;
}

int main(void) {
	signal(SIGINT, handler);

	hid_device *dev;
	struct hid_device_info *devs, *cur_dev;

	// Open HID device.
	char* path = NULL;
	devs = hid_enumerate(0x0, 0x0);
	cur_dev = devs;
	while(cur_dev) {
		if (cur_dev->vendor_id == 0x045e && cur_dev->product_id == 0x0773 && cur_dev->interface_number == 2) {
			path = cur_dev->path;
			break;
		}
		cur_dev = cur_dev->next;
	}
	if (!path) {
		fprintf(stderr, "Couldn't find TouchMouse, aborting\n");
		return -1;
	}
	dev = hid_open_path(path);
	if (!dev) {
		fprintf(stderr, "Failed to open device %s, aborting\n", path);
		return -1;
	}
	hid_free_enumeration(devs);

	// Enable image updates
	int res = 0;
	res = enable_mouse_image_mode(dev);
	if (res != 0) {
		fprintf(stderr, "Failed to enable full touch updates, aborting\n");
		return -1;
	}

	// Initialize decoder
	decoder* state = (decoder*)malloc(sizeof(decoder));
	memset(state, 0, sizeof(*state));
	state->cb = print_table;

	uint8_t last_timestamp = 0;

	// Poll for updates.
	unsigned char data[256] = {};
	printf("polling for image updates...\n");
	hid_set_nonblocking(dev, 1); // Enable nonblocking reads
	while(!quit) {
		res = hid_read_timeout(dev, data, 255, 100); // 100 msec is hardly noticable, but keeps us from pegging a CPU core
		if (res < 0 ) {
			fprintf(stderr, "hid_read() failed: %d\n", res);
			return -1;
		} else if (res > 0) {
			// Dump contents of transfer
			printf("Got reply: %d bytes:", res);
			int j;
			for(j = 0; j < res; j++) {
				printf(" %02X", data[j]);
			}
			printf("\n");
			// Interpret contents.
			report* r = (report*)data;
			// We only care about report ID 39 (0x27), which should be 32 bytes long
			if (res == 32 && r->report_id == 0x27) {
				printf("Timestamp: %02X\t%02X bytes:", r->timestamp, r->length - 1);
				int t;
				for(t = 0; t < r->length - 1; t++) {
					printf(" %02X", r->data[t]);
				}
				printf("\n");
				if (r->timestamp != last_timestamp) {
					reset_decoder(state); // Reset decoder for next transfer
					last_timestamp = r->timestamp;
				}
				for(t = 0; t < r->length - 1; t++) { // Note that we subtract one byte because the length includes the timestamp byte.
					int res;
					// Yes, we process the low nybble first.  Embedded systems are funny like that.
					res = process_nybble(state, r->data[t] & 0xf);
					if (res == DECODER_COMPLETE) {
						state->cb(state, state->image, r->timestamp);
						reset_decoder(state); // Reset decoder for next transfer
						break;
					}
					if (res == DECODER_ERROR) {
						fprintf(stderr, "Caught error in decoder, aborting!\n");
						goto cleanup;
					}
					res = process_nybble(state, (r->data[t] & 0xf0) >> 4);
					if (res == DECODER_COMPLETE) {
						state->cb(state, state->image, r->timestamp);
						reset_decoder(state); // Reset decoder for next transfer
						break;
					}
					if (res == DECODER_ERROR) {
						fprintf(stderr, "Caught error in decoder, aborting!\n");
						goto cleanup;
					}
				}
			}
		}
	}
cleanup:
	hid_close(dev);
	dev = NULL;
	hid_exit();
	return 0;
}

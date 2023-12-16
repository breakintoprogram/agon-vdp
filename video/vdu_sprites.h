#ifndef _VDU_SPRITES_H_
#define _VDU_SPRITES_H_

#include <fabgl.h>
#include <cmath>

#include "buffers.h"
#include "graphics.h"
#include "sprites.h"
#include "types.h"

// Sprite Engine, VDU command handler
//
void VDUStreamProcessor::vdu_sys_sprites() {
	auto cmd = readByte_t();

	switch (cmd) {
		case 0: {	// Select bitmap
			auto rb = readByte_t();
			if (rb >= 0) {
				setCurrentBitmap(rb + BUFFERED_BITMAP_BASEID);
				debug_log("vdu_sys_sprites: bitmap %d selected\n\r", getCurrentBitmapId());
			}
		}	break;

		case 1:	{	// Create new bitmap from stream (RGBA8888) or screen (Native, RGB222)
			auto rw = readWord_t(); if (rw == -1) return;
			auto rh = readWord_t(); if (rh == -1) return;

			if (rh == 0) {
				if (rw <= 255) {
					// capture bitmap from screen as per GXR
					// area defined from last two graphics move commands
					// bitmap ID sent in first rw byte
					createBitmapFromScreen(rw + BUFFERED_BITMAP_BASEID);
				} else {
					// invalid bitmap size definition for sending bitmap data stream
					debug_log("vdu_sys_sprites: bitmap %d - zero size\n\r", getCurrentBitmapId());
				}
				return;
			}

			receiveBitmap(getCurrentBitmapId(), rw, rh);
		}	break;

		case 2: {	// Define bitmap in single color
			auto rw = readWord_t(); if (rw == -1) return;
			auto rh = readWord_t(); if (rh == -1) return;
			uint32_t color;
			auto remaining = readIntoBuffer((uint8_t *)&color, sizeof(uint32_t));
			if (remaining > 0) {
				debug_log("vdu_sys_sprites: failed to receive color data\n\r");
				return;
			}

			createEmptyBitmap(getCurrentBitmapId(), rw, rh, color);
		}	break;

		case 3: {	// Draw bitmap to screen (x,y)
			auto rx = readWord_t(); if (rx == -1) return;
			auto ry = readWord_t(); if (ry == -1) return;

			drawBitmap(rx,ry);
			debug_log("vdu_sys_sprites: bitmap %d draw command\n\r", getCurrentBitmapId());
		}	break;

	   /*
		* Sprites
		* 
		* Sprite creation order:
		* 1) Create bitmap(s) for sprite, or re-use bitmaps already created
		* 2) Select the correct sprite ID (0-255). The GDU only accepts sequential sprite sets, starting from ID 0. All sprites must be adjacent to 0
		* 3) Clear out any frames from previous program definitions
		* 4) Add one or more frames to each sprite
		* 5) Activate sprite to the GDU
		* 6) Show sprites on screen / move them around as needed
		* 7) Refresh
		*/
		case 4: {	// Select sprite
			auto b = readByte_t(); if (b == -1) return;
			setCurrentSprite(b);
			debug_log("vdu_sys_sprites: sprite %d selected\n\r", getCurrentSprite());
		}	break;

		case 5: {	// Clear frames
			clearSpriteFrames();
			debug_log("vdu_sys_sprites: sprite %d - all frames cleared\n\r", getCurrentSprite());
		}	break;

		case 6:	{	// Add frame to sprite
			auto b = readByte_t(); if (b == -1) return;
			addSpriteFrame(b + BUFFERED_BITMAP_BASEID);
			debug_log("vdu_sys_sprites: sprite %d - bitmap %d added as frame %d\n\r", getCurrentSprite(), b, getSprite()->framesCount-1);
		}	break;

		case 7:	{	// Active sprites to GDU
			auto b = readByte_t(); if (b == -1) return;
			activateSprites(b);
			debug_log("vdu_sys_sprites: %d sprites activated\n\r", numsprites);
		}	break;

		case 8: {	// Set next frame on sprite
			nextSpriteFrame();
			debug_log("vdu_sys_sprites: sprite %d next frame\n\r", getCurrentSprite());
		}	break;

		case 9:	{	// Set previous frame on sprite
			previousSpriteFrame();
			debug_log("vdu_sys_sprites: sprite %d previous frame\n\r", getCurrentSprite());
		}	break;

		case 10: {	// Set current frame id on sprite
			auto b = readByte_t(); if (b == -1) return;
			setSpriteFrame(b);
			debug_log("vdu_sys_sprites: sprite %d set to frame %d\n\r", getCurrentSprite(), b);
		}	break;

		case 11: {	// Show sprite
			showSprite();
			debug_log("vdu_sys_sprites: sprite %d show cmd\n\r", getCurrentSprite());
		}	break;

		case 12: {	// Hide sprite
			hideSprite();
			debug_log("vdu_sys_sprites: sprite %d hide cmd\n\r", getCurrentSprite());
		}	break;

		case 13: {	// Move sprite to coordinate on screen
			auto rx = readWord_t(); if (rx == -1) return;
			auto ry = readWord_t(); if (ry == -1) return;
			moveSprite(rx, ry);
			debug_log("vdu_sys_sprites: sprite %d - move to (%d,%d)\n\r", getCurrentSprite(), rx, ry);
		}	break;

		case 14: {	// Move sprite by offset to current coordinate on screen
			auto rx = readWord_t(); if (rx == -1) return;
			auto ry = readWord_t(); if (ry == -1) return;
			moveSpriteBy(rx, ry);
			debug_log("vdu_sys_sprites: sprite %d - move by offset (%d,%d)\n\r", getCurrentSprite(), rx, ry);
		}	break;

		case 15: {	// Refresh
			refreshSprites();
			debug_log("vdu_sys_sprites: perform sprite refresh\n\r");
		}	break;

		case 16: {	// Reset
			resetSprites();
			resetBitmaps();
			cls(false);
			debug_log("vdu_sys_sprites: reset\n\r");
		}	break;

		case 17: {	// Reset sprites only
			resetSprites();
			debug_log("vdu_sys_sprites: reset sprites\n\r");
		}	break;

		// Extended bitmap commands
		case 0x20: {	// Select bitmap, 16-bit buffer ID
			auto b = readWord_t(); if (b == -1) return;
			setCurrentBitmap((uint16_t) b);
			debug_log("vdu_sys_sprites: bitmap %d selected\n\r", getCurrentBitmapId());
		}	break;

		case 0x21: {	// Create bitmap from buffer
			auto width = readWord_t(); if (width == -1) return;
			auto height = readWord_t(); if (height == -1) return;
			if (height == 0) {
				// capture bitmap from the screen, as per GXR
				// buffer ID sent in "width" word, format byte not required/ignored
				createBitmapFromScreen(width);
			} else {
				auto format = readByte_t(); if (format == -1) return;
				createBitmapFromBuffer(getCurrentBitmapId(), format, width, height);
			}
		}	break;

		case 0x26: {	// add sprite frame for bitmap (long ID)
			auto bufferId = readWord_t(); if (bufferId == -1) return;
			addSpriteFrame(bufferId);
			debug_log("vdu_sys_sprites: sprite %d - bitmap %d added as frame %d\n\r", getCurrentSprite(), bufferId, getSprite()->framesCount-1);
		}	break;

		case 0x40: {	// Setup mouse cursor from current bitmap
			auto hotX = readByte_t(); if (hotX == -1) return;
			auto hotY = readByte_t(); if (hotY == -1) return;
			if (makeCursor(getCurrentBitmapId(), hotX, hotY)) {
				debug_log("vdu_sys_sprites: cursor created from bitmap %d\n\r", getCurrentBitmapId());
			} else {
				debug_log("vdu_sys_sprites: cursor failed to create from bitmap %d\n\r", getCurrentBitmapId());
			}
		}	break;

		default: {
			debug_log("vdu_sys_sprites: unknown command %d\n\r", cmd);
		}	break;
	}
}

void VDUStreamProcessor::receiveBitmap(uint16_t bufferId, uint16_t width, uint16_t height) {
	// clear the buffer
	bufferClear(bufferId);
	// receive the data
	if (bufferWrite(bufferId, sizeof(uint32_t) * width * height) != 0) {
		// timed out, or couldn't allocate buffer - so abort
		return;
	}
	// create RGBA8888 bitmap from buffer
	createBitmapFromBuffer(bufferId, 0, width, height);
}

void VDUStreamProcessor::createBitmapFromScreen(uint16_t bufferId) {
	bufferClear(bufferId);
	// get screen rectangle from last two graphics cursor positions
	auto x1 = p1.X;
	auto y1 = p1.Y;
	auto x2 = p2.X;
	auto y2 = p2.Y;
	auto width = x2 - x1;
	auto height = y2 - y1;
	// normalize to positive values
	if (width < 0) {
		width = -width;
		x1 = x2;
	}
	if (height < 0) {
		height = -height;
		y1 = y2;
	}
	auto size = width * height;
	if (size == 0) {
		debug_log("vdu_sys_sprites: bitmap %d - zero size\n\r", bufferId);
		return;
	}

	// create a new buffer of appropriate size, and set as a native format bitmap
	auto buffer = bufferCreate(bufferId, size);
	if (!buffer) {
		debug_log("vdu_sys_sprites: failed to create buffer\n\r");
		return;
	}
	createBitmapFromBuffer(bufferId, 3, width, height);
	// Copy screen area to buffer
	canvas->copyToBitmap(x1, y1, getBitmap(bufferId).get());
}

void VDUStreamProcessor::createEmptyBitmap(uint16_t bufferId, uint16_t width, uint16_t height, uint32_t color) {
	bufferClear(bufferId);

	// create a new buffer of appropriate size
	uint32_t size = width * height;
	auto buffer = bufferCreate(bufferId, sizeof(uint32_t) * size);
	if (!buffer) {
		debug_log("vdu_sys_sprites: failed to create buffer\n\r");
		return;
	}
	auto dataptr = (uint32_t *)buffer->getBuffer();
	for (auto n = 0; n < size; n++) dataptr[n] = color;
	// create RGBA8888 bitmap from buffer
	createBitmapFromBuffer(bufferId, 0, width, height);
}

void VDUStreamProcessor::createBitmapFromBuffer(uint16_t bufferId, uint8_t format, uint16_t width, uint16_t height) {
	clearBitmap(bufferId);
	// do we have a buffer with this ID?
	if (buffers.find(bufferId) == buffers.end()) {
		debug_log("vdu_sys_sprites: buffer %d not found\n\r", bufferId);
		return;
	}
	// is this a singular buffer we can use for a bitmap source?
	if (buffers[bufferId].size() != 1) {
		debug_log("vdu_sys_sprites: buffer %d is not a singular buffer and cannot be used for a bitmap source\n\r", bufferId);
		return;
	}

	// create bitmap from buffer
	auto stream = buffers[bufferId][0];
	// map our pixel format, default to RGBA8888
	PixelFormat pixelFormat = PixelFormat::RGBA8888;
	auto bytesPerPixel = 4.;
	switch (format) {
		case 0:	// RGBA8888
			pixelFormat = PixelFormat::RGBA8888;
			bytesPerPixel = 4.;
			break;
		case 1:	// RGBA2222
			pixelFormat = PixelFormat::RGBA2222;
			bytesPerPixel = 1.;
			break;
		case 2: // Mono/Mask - TODO not sure exactly how to handle this
			pixelFormat = PixelFormat::Mask;
			bytesPerPixel = 1/8;
			break;
		case 3: // Native
			pixelFormat = PixelFormat::Native;
			bytesPerPixel = 1.;
			break;
		default:
			pixelFormat = PixelFormat::RGBA8888;
			bytesPerPixel = 4.;
			debug_log("vdu_sys_sprites: buffer %d - unknown pixel format %d, defaulting to RGBA8888\n\r", bufferId, format);
	}
	// Check that our stream length matches the expected length
	auto streamLength = stream->size();
	auto expectedLength = width * height * bytesPerPixel;
	if (streamLength != expectedLength) {
		debug_log("vdu_sys_sprites: buffer %d - stream length %d does not match expected length %f\n\r", bufferId, streamLength, expectedLength);
		return;
	}
	auto data = stream->getBuffer();
	bitmaps[bufferId] = make_shared_psram<Bitmap>(width, height, (uint8_t *)data, pixelFormat);
	debug_log("vdu_sys_sprites: bitmap created for bufferId %d, format %d, (%dx%d)\n\r", bufferId, format, width, height);
}

#endif // _VDU_SPRITES_H_

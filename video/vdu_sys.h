#ifndef VDU_SYS_H
#define VDU_SYS_H

#include <Arduino.h>
#include <fabgl.h>
#include <ESP32Time.h>

#include "agon.h"
#include "agon_fonts.h"
#include "cursor.h"
#include "vdp_protocol.h"

extern fabgl::PS2Controller PS2Controller;
extern int VGAColourDepth;					// Number of colours per pixel (2, 4,8, 16 or 64)
extern Point p1, p2, p3;						// Coordinate store for plot
extern RGB888 gfg, gbg;						// Graphics foreground and background colour
extern RGB888 tfg, tbg;						// Text foreground and background colour
extern uint8_t palette[64];					// Storage for the palette
extern const RGB888 colourLookup[64];		// Lookup table for VGA colours
extern int videoMode;						// Current video mode
extern int kbRepeatDelay;					// Keyboard repeat delay ms (250, 500, 750 or 1000)		
extern int kbRepeatRate;					// Keyboard repeat rate ms (between 33 and 500)
extern bool legacyModes;					// Default legacy modes being false
extern bool doubleBuffered;					// Disable double buffering by default
extern void switchTerminalMode();			// Switch to terminal mode
extern void vdu_sys_sprites();				// Sprite handler
extern void vdu_sys_audio();				// Audio handler
extern word play_note(byte channel, byte volume, word frequency, word duration);

bool 			initialised = false;			// Is the system initialised yet?
ESP32Time		rtc(0);							// The RTC


//// TODO: move to text.h file

bool cmp_char(uint8_t * c1, uint8_t *c2, int len) {
	for (int i = 0; i < len; i++) {
		if (*c1++ != *c2++) {
			return false;
		}
	}
	return true;
}

// Try and match a character
//
char get_screen_char(int px, int py) {
	RGB888	pixel;
	uint8_t	charRow;
	uint8_t	charData[8];
	uint8_t R = tbg.R;
	uint8_t G = tbg.G;
	uint8_t B = tbg.B;

	// Do some bounds checking first
	//
	if (px < 0 || py < 0 || px >= canvasW - 8 || py >= canvasH - 8) {
		return 0;
	}

	// Now scan the screen and get the 8 byte pixel representation in charData
	//
	for (int y = 0; y < 8; y++) {
		charRow = 0;
		for (int x = 0; x < 8; x++) {
			pixel = Canvas->getPixel(px + x, py + y);
			if (!(pixel.R == R && pixel.G == G && pixel.B == B)) {
				charRow |= (0x80 >> x);
			}
		}
		charData[y] = charRow;
	}
	//
	// Finally try and match with the character set array
	//
	for (int i = 32; i <= 255; i++) {
		if (cmp_char(charData, &fabgl::FONT_AGON_DATA[i * 8], 8)) {	
			return i;		
		}
	}
	return 0;
}



// VDU 23, 0, &80, <echo>: Send a general poll/echo byte back to MOS
//
void sendGeneralPoll() {
	byte b = readByte_b();
	byte packet[] = {
		b,
	};
	send_packet(PACKET_GP, sizeof packet, packet);
	initialised = true;	
}

// VDU 23, 0, &81, <region>: Set the keyboard layout
//
void vdu_sys_video_kblayout() {
	int	region = readByte_t();			// Fetch the region
	// TODO refactor out setting layout to keyboard handling file
	switch(region) {
		case 1:	PS2Controller.keyboard()->setLayout(&fabgl::USLayout); break;
		case 2:	PS2Controller.keyboard()->setLayout(&fabgl::GermanLayout); break;
		case 3:	PS2Controller.keyboard()->setLayout(&fabgl::ItalianLayout); break;
		case 4:	PS2Controller.keyboard()->setLayout(&fabgl::SpanishLayout); break;
		case 5: PS2Controller.keyboard()->setLayout(&fabgl::FrenchLayout); break;
		case 6:	PS2Controller.keyboard()->setLayout(&fabgl::BelgianLayout); break;
		case 7:	PS2Controller.keyboard()->setLayout(&fabgl::NorwegianLayout); break;
		case 8:	PS2Controller.keyboard()->setLayout(&fabgl::JapaneseLayout);break;
		default:
			PS2Controller.keyboard()->setLayout(&fabgl::UKLayout);
			break;
	}
}

// VDU 23, 0, &82: Send the cursor position back to MOS
//
void sendCursorPosition() {
	byte packet[] = {
		textCursor.X / fontW,
		textCursor.Y / fontH,
	};
	send_packet(PACKET_CURSOR, sizeof packet, packet);	
}
	
// VDU 23, 0, &83 Send a character back to MOS
//
void sendScreenChar(int x, int y) {
	int	px = x * fontW;
	int py = y * fontH;
	char c = get_screen_char(px, py);
	byte packet[] = {
		c,
	};
	send_packet(PACKET_SCRCHAR, sizeof packet, packet);
}

// VDU 23, 0, &84: Send a pixel value back to MOS
//
void sendScreenPixel(int x, int y) {
	// TODO refactor VGA and colour things
	RGB888	pixel;
	byte 	pixelIndex = 0;
	Point	p = translateViewport(scale(x, y));
	//
	// Do some bounds checking first
	//
	if (p.X >= 0 && p.Y >= 0 && p.X < canvasW && p.Y < canvasH) {
		pixel = Canvas->getPixel(p.X, p.Y);
		for (byte i = 0; i < VGAColourDepth; i++) {
			if (colourLookup[palette[i]] == pixel) {
				pixelIndex = i;
				break;
			}
		}
	}	
	byte packet[] = {
		pixel.R,	// Send the colour components
		pixel.G,
		pixel.B,
		pixelIndex,	// And the pixel index in the palette
	};
	send_packet(PACKET_SCRPIXEL, sizeof packet, packet);	
}

// VDU 23, 0, &86: Send MODE information (screen details)
//
void sendModeInformation() {
	// TODO refactor VGA and mode things
	byte packet[] = {
		canvasW & 0xFF,	 					// Width in pixels (L)
		(canvasW >> 8) & 0xFF,				// Width in pixels (H)
		canvasH & 0xFF,						// Height in pixels (L)
		(canvasH >> 8) & 0xFF,				// Height in pixels (H)
		canvasW / fontW,					// Width in characters (byte)
		canvasH / fontH,					// Height in characters (byte)
		VGAColourDepth,						// Colour depth
		videoMode,							// The video mode number
	};
	send_packet(PACKET_MODE, sizeof packet, packet);
}

// Send TIME information (from ESP32 RTC)
//
void sendTime() {
	byte packet[] = {
		rtc.getYear() - EPOCH_YEAR,			// 0 - 255
		rtc.getMonth(),						// 0 - 11
		rtc.getDay(),						// 1 - 31
		rtc.getDayofYear(),					// 0 - 365
		rtc.getDayofWeek(),					// 0 - 6
		rtc.getHour(true),					// 0 - 23
		rtc.getMinute(),					// 0 - 59
		rtc.getSecond(),					// 0 - 59
	};
	send_packet(PACKET_RTC, sizeof packet, packet);
}

// VDU 23, 0, &87, <mode>, [args]: Handle time requests
//
void vdu_sys_video_time() {
	int mode = readByte_t();

	if (mode == 0) {
		sendTime();
	}
	else if (mode == 1) {
		int yr = readByte_t(); if (yr == -1) return;
		int mo = readByte_t(); if (mo == -1) return;
		int da = readByte_t(); if (da == -1) return;
		int ho = readByte_t(); if (ho == -1) return;	
		int mi = readByte_t(); if (mi == -1) return;
		int se = readByte_t(); if (se == -1) return;

		yr = EPOCH_YEAR + (int8_t)yr;

		if (yr >= 1970) {
			rtc.setTime(se, mi, ho, da, mo, yr);
		}
	}
}

// Send the keyboard state
//
void sendKeyboardState() {
	bool numLock;
	bool capsLock;
	bool scrollLock;
	PS2Controller.keyboard()->getLEDs(&numLock, &capsLock, &scrollLock);
	byte packet[] = {
		kbRepeatDelay & 0xFF,
		(kbRepeatDelay >> 8) & 0xFF,
		kbRepeatRate & 0xFF,
		(kbRepeatRate >> 8) & 0xFF,
		scrollLock | (capsLock << 1) | (numLock << 2)
	};
	send_packet(PACKET_KEYSTATE, sizeof packet, packet);
}

// VDU 23, 0, &88, delay; repeatRate; LEDs: Handle the keystate
// Send 255 for LEDs to leave them unchanged
//
void vdu_sys_keystate() {
	// TODO refactor keyboard handling things to keyboard.h
	int d = readWord_t(); if (d == -1) return;
	int r = readWord_t(); if (r == -1) return;
	int b = readByte_t(); if (b == -1) return;

	if (d >= 250 && d <= 1000) kbRepeatDelay = (d / 250) * 250;	// In steps of 250ms
	if (r >=  33 && r <=  500) kbRepeatRate  = r;

	if (b != 255) {
		PS2Controller.keyboard()->setLEDs(b & 4, b & 2, b & 1);
	}
	PS2Controller.keyboard()->setTypematicRateAndDelay(kbRepeatRate, kbRepeatDelay);
	debug_log("vdu_sys_video: keystate: d=%d, r=%d, led=%d\n\r", kbRepeatDelay, kbRepeatRate, b);
	sendKeyboardState();
}

// VDU 23,0: VDP control
// These can send responses back; the response contains a packet # that matches the VDU command mode byte
//
void vdu_sys_video() {
  	int	mode = readByte_t();

  	switch (mode) {
		case VDP_GP: {					// VDU 23, 0, &80
			sendGeneralPoll();			// Send a general poll packet
		}	break;
		case VDP_KEYCODE: {				// VDU 23, 0, &81, layout
			vdu_sys_video_kblayout();
		}	break;
		case VDP_CURSOR: {				// VDU 23, 0, &82
			sendCursorPosition();		// Send cursor position
		}	break;
		case VDP_SCRCHAR: {				// VDU 23, 0, &83, x; y;
			int x = readWord_t();		// Get character at screen position x, y
			int y = readWord_t();
			sendScreenChar(x, y);
		}	break;
		case VDP_SCRPIXEL: {			// VDU 23, 0, &84, x; y;
			int x = readWord_t();		// Get pixel value at screen position x, y
			int y = readWord_t();
			sendScreenPixel((short)x, (short)y);
		} 	break;		
		case VDP_AUDIO: {				// VDU 23, 0, &85, channel, command, <args>
			vdu_sys_audio();
		}	break;
		case VDP_MODE: {				// VDU 23, 0, &86
			sendModeInformation();		// Send mode information (screen dimensions, etc)
		}	break;
		case VDP_RTC: {					// VDU 23, 0, &87, mode
			vdu_sys_video_time();		// Send time information
		}	break;
		case VDP_KEYSTATE: {			// VDU 23, 0, &88, repeatRate; repeatDelay; status
			vdu_sys_keystate();
		}	break;
		case VDP_LOGICALCOORDS: {		// VDU 23, 0, &C0, n
			int b = readByte_t();		// Set logical coord mode
			if (b >= 0) {
				logicalCoords = b;	
			}
		}	break;
		case VDP_LEGACYMODES: {			// VDU 23, 0, &C1, n
			int b = readByte_t();		// Switch legacy modes on or off
			if (b == 0) {
				legacyModes = false;
			} else {
				legacyModes = true;
			}
		}	break;
		case VDP_SWITCHBUFFER: {		// VDU 23, 0, &C3
			if (doubleBuffered == true) {
				Canvas->swapBuffers();
			}
		}	break;
		case VDP_TERMINALMODE: {		// VDU 23, 0, &FF
			switchTerminalMode(); 		// Switch to terminal mode
		}	break;
  	}
}

// VDU 23,7: Scroll rectangle on screen
//
void vdu_sys_scroll() {
	// TODO refactor to use scrolling handling inside viewport.h
	int extent = readByte_t(); 		if(extent == -1) return;	// Extent (0 = text viewport, 1 = entire screen, 2 = graphics viewport)
	int direction = readByte_t();	if(direction == -1) return;	// Direction
	int movement = readByte_t();	if(movement == -1) return;	// Number of pixels to scroll

	Rect * region;

	switch(extent) {
		case 0:		// Text viewport
			region = &textViewport;
			break;
		case 2: 	// Graphics viewport
			region = &graphicsViewport;
			break;
		default:	// Entire screen
			region = &defaultViewport;
			break;
	}
	Canvas->setScrollingRegion(region->X1, region->Y1, region->X2, region->Y2);

	switch(direction) {
		case 0:	// Right
			Canvas->scroll(movement, 0);
			break;
		case 1: // Left
			Canvas->scroll(-movement, 0);
			break;
		case 2: // Down
			Canvas->scroll(0, movement);
			break;
		case 3: // Up
			Canvas->scroll(0, -movement);
			break;
	}
	Canvas->waitCompletion(false);
}


// VDU 23,16: Set cursor behaviour
// 
void vdu_sys_cursorBehaviour() {
	int setting = readByte_t();	if (setting == -1) return;
	int mask = readByte_t();	if (mask == -1) return;

	setCursorBehaviour((byte) setting, (byte) mask);
}

// VDU 23, char, n1, n2, n3, n4, n5, n6, n7, n8: Redefine a display character
// Parameters:
// - c: The character to redefine
//
void vdu_sys_udg(byte c) {
	uint8_t		buffer[8];
	int			b;

	for(int i = 0; i < 8; i++) {
		b = readByte_t();
		if(b == -1) {
			return;
		}
		buffer[i] = b;
	}	
	// TODO refactor character redefinition to font handling file
	memcpy(&fabgl::FONT_AGON_DATA[c * 8], buffer, 8);
}


// Handle SYS
// VDU 23,mode
//
void vdu_sys() {
  	int mode = readByte_t();

	//
	// If mode is -1, then there's been a timeout
	//
	if (mode == -1) {
		return;
	}
	//
	// If mode < 32, then it's a system command
	//
	else if (mode < 32) {
  		switch(mode) {
			case 0x00: {					// VDU 23, 0
	  			vdu_sys_video();			// Video system control
			}	break;
			case 0x01: {					// VDU 23, 1
				int b = readByte_t();		// Cursor control
				if (b >= 0) {
					enableCursor((bool) b);
				}
			}	break;
			case 0x07: {					// VDU 23, 7
				vdu_sys_scroll();			// Scroll 
			}	break;
			case 0x10: {					// VDU 23, 16
				vdu_sys_cursorBehaviour();	// Set cursor behaviour
				break;
			}
			case 0x1B: {					// VDU 23, 27
				vdu_sys_sprites();			// Sprite system control
			}	break;
  		}
	}
	//
	// Otherwise, do
	// VDU 23,mode,n1,n2,n3,n4,n5,n6,n7,n8
	// Redefine character with ASCII code mode
	//
	else {
		vdu_sys_udg(mode);
	}
}

// Wait for eZ80 to initialise
//
void wait_eZ80() {
	debug_log("wait_eZ80: Start\n\r");
	while (!initialised) {
		if (byteAvailable()) {
			byte c = readByte();	// Only handle VDU 23 packets
			if (c == 23) {
				vdu_sys();
			}
		}
	}
	debug_log("wait_eZ80: End\n\r");
}

#endif // VDU_SYS_H

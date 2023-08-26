//
// Title:	        Agon Video BIOS
// Author:        	Dean Belfield
// Contributors:	Jeroen Venema (Sprite Code, VGA Mode Switching)
//					Damien Guard (Fonts)
//					Igor Chaves Cananea (vdp-gl maintenance)
// Created:       	22/03/2022
// Last Updated:	13/08/2023
//
// Modinfo:
// 11/07/2022:		Baud rate tweaked for Agon Light, HW Flow Control temporarily commented out
// 26/07/2022:		Added VDU 29 support
// 03/08/2022:		Set codepage 1252, fixed keyboard mappings for AGON, added cursorTab, VDP serial protocol
// 06/08/2022:		Added a custom font, fixed UART initialisation, flow control
// 10/08/2022:		Improved keyboard mappings, added sprites, audio, new font
// 05/09/2022:		Moved the audio class to agon_audio.h, added function prototypes in agon.h
// 02/10/2022:		Version 1.00: Tweaked the sprite code, changed bootup title to Quark
// 04/10/2022:		Version 1.01: Can now change keyboard layout, origin and sprites reset on mode change, available modes tweaked
// 23/10/2022:		Version 1.02: Bug fixes, cursor visibility and scrolling
// 15/02/2023:		Version 1.03: Improved mode, colour handling and international support
// 04/03/2023:					+ Added logical screen resolution, sendScreenPixel now sends palette index as well as RGB values
// 09/03/2023:					+ Keyboard now sends virtual key data, improved VDU 19 to handle COLOUR l,p as well as COLOUR l,r,g,b
// 15/03/2023:					+ Added terminal support for CP/M, RTC support for MOS
// 21/03/2023:				RC2 + Added keyboard repeat delay and rate, logical coords now selectable
// 22/03/2023:					+ VDP control codes now indexed from 0x80, added paged mode (VDU 14/VDU 15)
// 23/03/2023:					+ Added VDP_GP
// 26/03/2023:				RC3 + Potential fixes for FabGL being overwhelmed by faster comms
// 27/03/2023:					+ Fix for sprite system crash
// 29/03/2023:					+ Typo in boot screen fixed
// 01/04/2023:					+ Added resetPalette to MODE, timeouts to VDU commands
// 08/04/2023:				RC4 + Removed delay in readbyte_t, fixed VDP_SCRCHAR, VDP_SCRPIXEL
// 12/04/2023:					+ Fixed bug in play_note
// 13/04/2023:					+ Fixed bootup fail with no keyboard
// 17/04/2023:				RC5 + Moved wait_completion in vdu so that it only executes after graphical operations
// 18/04/2023:					+ Minor tweaks to wait completion logic
// 12/05/2023:		Version 1.04: Now uses vdp-gl instead of FabGL, implemented GCOL mode, sendModeInformation now sends video mode
// 19/05/2023:					+ Added VDU 4/5 support
// 25/05/2023:					+ Added VDU 24, VDU 26 and VDU 28, fixed inverted text colour settings
// 30/05/2023:					+ Added VDU 23,16 (cursor movement control)
// 28/06/2023:					+ Improved get_screen_char, fixed vdu_textViewport, cursorHome, changed modeline for Mode 2
// 30/06/2023:					+ Fixed vdu_sys_sprites to correctly discard serial input if bitmap allocation fails
// 13/08/2023:					+ New video modes, mode change resets page mode

#include <fabgl.h>

#define VERSION			1
#define REVISION		4
#define RC				1

#define	DEBUG			0						// Serial Debug Mode: 1 = enable
#define SERIALKB		0						// Serial Keyboard: 1 = enable (Experimental)

fabgl::PS2Controller		PS2Controller;		// The keyboard class
fabgl::Canvas *				Canvas;				// The canvas class

fabgl::VGA2Controller		VGAController2;		// VGA class - 2 colours
fabgl::VGA4Controller		VGAController4;		// VGA class - 4 colours
fabgl::VGA8Controller		VGAController8;		// VGA class - 8 colours
fabgl::VGA16Controller		VGAController16;	// VGA class - 16 colours
fabgl::VGAController		VGAController64;	// VGA class - 64 colours

fabgl::VGABaseController *	VGAController;		// Pointer to the current VGA controller class (one of the above)

fabgl::Terminal				Terminal;			// Used for CP/M mode

fabgl::PaintOptions			gpo;				// Graphics paint options
fabgl::PaintOptions			tpo;				// Text paint options

#include "agon.h"								// Configuration file
#include "agon_fonts.h"							// The Acorn BBC Micro Font
#include "agon_palette.h"						// Colour lookup table
#include "vdp_protocol.h"						// VDP Protocol
#include "vdu.h"								// VDU functions
#include "vdu_audio.h"							// Audio support
#include "viewport.h"							// Viewport support

int				VGAColourDepth;					// Number of colours per pixel (2, 4,8, 16 or 64)
Point       	p1, p2, p3;						// Coordinate store for plot
RGB888    		gfg, gbg;						// Graphics foreground and background colour
RGB888      	tfg, tbg;						// Text foreground and background colour
int				fontW;							// Font width
int				fontH;							// Font height
int         	count = 0;						// Generic counter, incremented every iteration of loop
uint8_t			numsprites = 0;					// Number of sprites on stage
uint8_t 		current_sprite = 0; 			// Current sprite number
uint8_t 		current_bitmap = 0;				// Current bitmap number
Bitmap			bitmaps[MAX_BITMAPS];			// Bitmap object storage
Sprite			sprites[MAX_SPRITES];			// Sprite object storage
byte 			keycode = 0;					// Last pressed key code
byte 			modifiers = 0;					// Last pressed key modifiers
bool			terminalMode = false;			// Terminal mode
int				videoMode;						// Current video mode
int				kbRepeatDelay = 500;			// Keyboard repeat delay ms (250, 500, 750 or 1000)		
int				kbRepeatRate = 100;				// Keyboard repeat rate ms (between 33 and 500)
bool			doWaitCompletion;				// For vdu function
uint8_t			palette[64];					// Storage for the palette
bool			legacyModes = false;			// Default legacy modes being false
bool			doubleBuffered = false;			// Disable double buffering by default

#if DEBUG == 1 || SERIALKB == 1
HardwareSerial DBGSerial(0);
#endif 

void setup() {
	disableCore0WDT(); delay(200);								// Disable the watchdog timers
	disableCore1WDT(); delay(200);
	#if DEBUG == 1 || SERIALKB == 1
	DBGSerial.begin(500000, SERIAL_8N1, 3, 1);
	#endif 
	setupVDPProtocol();
	wait_eZ80();
 	PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
	PS2Controller.keyboard()->setLayout(&fabgl::UKLayout);
	PS2Controller.keyboard()->setCodePage(fabgl::CodePages::get(1252));
	PS2Controller.keyboard()->setTypematicRateAndDelay(kbRepeatRate, kbRepeatDelay);
	init_audio();
	copy_font();
  	set_mode(1);
	boot_screen();
}

// The main loop
//
void loop() {
	bool cursorVisible = false;
	bool cursorState = false;

	while (true) {
		if(terminalMode) {
			do_keyboard_terminal();
			continue;
		}
    	cursorVisible = ((count & 0xFFFF) == 0);
    	if(cursorVisible) {
      		cursorState = !cursorState;
      		do_cursor();
    	}
    	do_keyboard();
    	if (byteAvailable()) {
      		if (cursorState) {
 	    		cursorState = false;
        		do_cursor();
      		}
      		byte c = readByte();
      		vdu(c);
    	}
    	count++;
  	}
}

// The boot screen
//
void boot_screen() {
  	printFmt("Agon Quark VDP Version %d.%02d", VERSION, REVISION);
	#if RC > 0
	  	printFmt(" RC%d", RC);
	#endif
	printFmt("\n\r");
}

// Debug printf to PC
//
void debug_log(const char *format, ...) {
	#if DEBUG == 1
   	va_list ap;
   	va_start(ap, format);
   	int size = vsnprintf(nullptr, 0, format, ap) + 1;
   	if (size > 0) {
    	va_end(ap);
     	va_start(ap, format);
     	char buf[size + 1];
     	vsnprintf(buf, size, format, ap);
     	DBGSerial.print(buf);
   	}
   	va_end(ap);
	#endif
}

// Copy the AGON font data from Flash to RAM
//
void copy_font() {
	memcpy(fabgl::FONT_AGON_DATA + 256, fabgl::FONT_AGON_BITMAP, sizeof(fabgl::FONT_AGON_BITMAP));
}

// Clear the screen
// 
void cls(bool resetViewports) {
	if (resetViewports) {
		viewportReset();
	}
	if (Canvas) {
		Canvas->setPenColor(tfg);
 		Canvas->setBrushColor(tbg);	
		Canvas->setPaintOptions(tpo);
		clearViewport(&textViewport);
	}
	if (numsprites) {
		if(VGAController) {
			VGAController->removeSprites();
			clearViewport(&textViewport);
		}
		numsprites = 0;
	}
	homeCursor();
	setPagedMode();
}

// Clear the graphics area
//
void clg() {
	if (Canvas) {
		Canvas->setPenColor(gfg);
 		Canvas->setBrushColor(gbg);	
		Canvas->setPaintOptions(gpo);
		clearViewport(&graphicsViewport);
	}
}

// Switch to terminal mode
//
void switchTerminalMode() {
	cls(true);
  	delete Canvas;
	Terminal.begin(VGAController);	
	Terminal.connectSerialPort(VDPSerial);
	Terminal.enableCursor(true);
	terminalMode = true;
}

// Get controller
// Parameters:
// - colours: Number of colours per pixel (2, 4, 8, 16 or 64)
// Returns:
// - A singleton instance of a VGAController class
//
fabgl::VGABaseController * get_VGAController(int colours) {
	switch(colours) {
		case  2: return VGAController2.instance();
		case  4: return VGAController4.instance();
		case  8: return VGAController8.instance();
		case 16: return VGAController16.instance();
		case 64: return VGAController64.instance();
	}
	return nullptr;
}

// Update the internal FabGL LUT
//
void updateRGB2PaletteLUT() {
	switch(VGAColourDepth) {
		case 2: VGAController2.updateRGB2PaletteLUT(); break;
		case 4: VGAController4.updateRGB2PaletteLUT(); break;
		case 8: VGAController8.updateRGB2PaletteLUT(); break;
		case 16: VGAController16.updateRGB2PaletteLUT(); break;
	}
}

// Reset the palette and set the foreground and background drawing colours
// Parameters:
// - colou: Array of indexes into colourLookup table
// - sizeOfArray: Size of passed colours array
//
void resetPalette(const uint8_t colours[]) {
	for(int i = 0; i < 64; i++) {
		uint8_t c = colours[i%VGAColourDepth];
		palette[i] = c;
		setPaletteItem(i, colourLookup[c]);
	}
	updateRGB2PaletteLUT();
}

// Change video resolution
// Parameters:
// - colours: Number of colours per pixel (2, 4, 8, 16 or 64)
// - modeLine: A modeline string (see the FagGL documentation for more details)
// Returns:
// - 0: Successful
// - 1: Invalid # of colours
// - 2: Not enough memory for mode
//
int change_resolution(int colours, char * modeLine) {
	fabgl::VGABaseController * controller = get_VGAController(colours);

	if(controller == nullptr) {						// If controller is null, then an invalid # of colours was passed
		return 1;									// So return the error
	}
  	delete Canvas;									// Delete the canvas

	VGAColourDepth = colours;						// Set the number of colours per pixel
	if(VGAController != controller) {				// Is it a different controller?
		if(VGAController) {							// If there is an existing controller running then
			VGAController->end();					// end it
		}
		VGAController = controller;					// Switch to the new controller
		VGAController->begin();						// And spin it up
	}
	if(modeLine) {									// If modeLine is not a null pointer then
		if (doubleBuffered == true) {
			VGAController->setResolution(modeLine, -1, -1, 1);
		} else {
			VGAController->setResolution(modeLine);		// Set the resolution
		}
	}
	VGAController->enableBackgroundPrimitiveExecution(true);
	VGAController->enableBackgroundPrimitiveTimeout(false);

  	Canvas = new fabgl::Canvas(VGAController);		// Create the new canvas
	//
	// Check whether the selected mode has enough memory for the vertical resolution
	//
	if(VGAController->getScreenHeight() != VGAController->getViewPortHeight()) {
		return 2;
	}
	return 0;										// Return with no errors
}

// Do the mode change
// Parameters:
// - mode: The video mode
// Returns:
// -  0: Successful
// -  1: Invalid # of colours
// -  2: Not enough memory for mode
// - -1: Invalid mode
//
int change_mode(int mode) {
	int errVal = -1;

	doubleBuffered = false;			// Default is to not double buffer the display

	cls(true);
	if(mode != videoMode) {
		switch(mode) {
			case 0:{
					if (legacyModes == true) {
						errVal = change_resolution(2, SVGA_1024x768_60Hz);
					} else {
						errVal = change_resolution(16, VGA_640x480_60Hz);	// VDP 1.03 Mode 3, VGA Mode 12h
					}
				}
				break;
			case 1:{
					if (legacyModes == true) {
						errVal = change_resolution(16, VGA_512x384_60Hz);
					} else {
						errVal = change_resolution(4, VGA_640x480_60Hz);
					}
				}
				break;
			case 2:{
					if (legacyModes == true) {
						errVal = change_resolution(64, VGA_320x200_75Hz);
					} else {
						errVal = change_resolution(2, VGA_640x480_60Hz);
					}
				}
				break;
			case 3: {
					if (legacyModes == true) {
						errVal = change_resolution(16, VGA_640x480_60Hz);
					} else {
						errVal = change_resolution(64, VGA_640x240_60Hz);
					}
				}
				break;
			case 4:
				errVal = change_resolution(16, VGA_640x240_60Hz);
				break;
			case 5:
				errVal = change_resolution(4, VGA_640x240_60Hz);
				break;
			case 6:
				errVal = change_resolution(2, VGA_640x240_60Hz);
				break;
			case 8:
				errVal = change_resolution(64, QVGA_320x240_60Hz);		// VGA "Mode X"
				break;
			case 9:
				errVal = change_resolution(16, QVGA_320x240_60Hz);
				break;
			case 10:
				errVal = change_resolution(4, QVGA_320x240_60Hz);
				break;
			case 11:
				errVal = change_resolution(2, QVGA_320x240_60Hz);
				break;
			case 12:
				errVal = change_resolution(64, VGA_320x200_70Hz);		// VGA Mode 13h
				break;
			case 13:
				errVal = change_resolution(16, VGA_320x200_70Hz);
				break;
			case 14:
				errVal = change_resolution(4, VGA_320x200_70Hz);
				break;
			case 15:
				errVal = change_resolution(2, VGA_320x200_70Hz);
				break;
			case 16:
				errVal = change_resolution(4, SVGA_800x600_60Hz);
				break;
			case 17:
				errVal = change_resolution(2, SVGA_800x600_60Hz);
				break;
			case 18:
				errVal = change_resolution(2, SVGA_1024x768_60Hz);		// VDP 1.03 Mode 0
				break;
			case 129:
				doubleBuffered = true;
				errVal = change_resolution(4, VGA_640x480_60Hz);
				break;
			case 130:
				doubleBuffered = true;
				errVal = change_resolution(2, VGA_640x480_60Hz);
				break;
			case 132:
				doubleBuffered = true;
				errVal = change_resolution(16, VGA_640x240_60Hz);
				break;
			case 133:
				doubleBuffered = true;
				errVal = change_resolution(4, VGA_640x240_60Hz);
				break;
			case 134:
				doubleBuffered = true;
				errVal = change_resolution(2, VGA_640x240_60Hz);
				break;
			case 136:
				doubleBuffered = true;
				errVal = change_resolution(64, QVGA_320x240_60Hz);		// VGA "Mode X"
				break;
			case 137:
				doubleBuffered = true;
				errVal = change_resolution(16, QVGA_320x240_60Hz);
				break;
			case 138:
				doubleBuffered = true;
				errVal = change_resolution(4, QVGA_320x240_60Hz);
				break;	
			case 139:
				doubleBuffered = true;
				errVal = change_resolution(2, QVGA_320x240_60Hz);
				break;
			case 140:
				doubleBuffered = true;
				errVal = change_resolution(64, VGA_320x200_70Hz);		// VGA Mode 13h
				break;
			case 141:
				doubleBuffered = true;
				errVal = change_resolution(16, VGA_320x200_70Hz);
				break;
			case 142:
				doubleBuffered = true;
				errVal = change_resolution(4, VGA_320x200_70Hz);
				break;
			case 143:
				doubleBuffered = true;
				errVal = change_resolution(2, VGA_320x200_70Hz);
				break;									
		}
		if(errVal != 0) {
			return errVal;
		}
	}
	switch(VGAColourDepth) {
		case  2: resetPalette(defaultPalette02); break;
		case  4: resetPalette(defaultPalette04); break;
		case  8: resetPalette(defaultPalette08); break;
		case 16: resetPalette(defaultPalette10); break;
		case 64: resetPalette(defaultPalette40); break;
	}
	tpo = getPaintOptions(0, tpo);
	gpo = getPaintOptions(0, gpo);
 	gfg = colourLookup[0x3F];
	gbg = colourLookup[0x00];
	tfg = colourLookup[0x3F];
	tbg = colourLookup[0x00];
  	Canvas->selectFont(&fabgl::FONT_AGON);
  	Canvas->setGlyphOptions(GlyphOptions().FillBackground(true));
  	Canvas->setPenWidth(1);
	setOrigin(0,0);
	p1 = Point(0,0);
	p2 = Point(0,0);
	p3 = Point(0,0);
	canvasW = Canvas->getWidth();
	canvasH = Canvas->getHeight();
	fontW = Canvas->getFontInfo()->width;
	fontH = Canvas->getFontInfo()->height;
	logicalScaleX = LOGICAL_SCRW / (double)canvasW;
	logicalScaleY = LOGICAL_SCRH / (double)canvasH;
	resetCursor();
	viewportReset();
	sendModeInformation();
	debug_log("do_modeChange: canvas(%d,%d), scale(%f,%f)\n\r", canvasW, canvasH, logicalScaleX, logicalScaleY);
	return 0;
}

// Change the video mode
// If there is an error, restore the last mode
// Parameters:
// - mode: The video mode
// 
void set_mode(int mode) {
	int errVal = change_mode(mode);
	if(errVal != 0) {
		debug_log("set_mode: error %d\n\r", errVal);
		change_mode(videoMode);
		return;
	}
	videoMode = mode;
}

void print(char const * text) {
	for(int i = 0; i < strlen(text); i++) {
    	vdu(text[i]);
  	}
}

void printFmt(const char *format, ...) {
   	va_list ap;
   	va_start(ap, format);
   	int size = vsnprintf(nullptr, 0, format, ap) + 1;
   	if (size > 0) {
    	va_end(ap);
     	va_start(ap, format);
     	char buf[size + 1];
     	vsnprintf(buf, size, format, ap);
     	print(buf);
   	}
   	va_end(ap);
 }

// Handle the keyboard: CP/M Terminal Mode
// 
void do_keyboard_terminal() {
  	fabgl::Keyboard* kb = PS2Controller.keyboard();
	fabgl::VirtualKeyItem item;

	// Read the keyboard and transmit to the Z80
	//
	if(kb->getNextVirtualKey(&item, 0)) {
		if(item.down) {
			writeByte(item.ASCII);
		}
	}

	// Wait for the response and write to the screen
	//
	while(byteAvailable()) {
		Terminal.write(readByte());
	}
}

// Wait for shift key to be released, then pressed (for paged mode)
// 
void wait_shiftkey() {
  	fabgl::Keyboard* kb = PS2Controller.keyboard();
	fabgl::VirtualKeyItem item;

	// Wait for shift to be released
	//
	do {
		kb->getNextVirtualKey(&item, 0);
	} while(item.SHIFT);

	// And pressed again
	//
	do {
		kb->getNextVirtualKey(&item, 0);
		if(item.ASCII == 27) {	// Check for ESC
			byte packet[] = {
				item.ASCII,
				0,
				item.vk,
				item.down,
			};
			send_packet(PACKET_KEYCODE, sizeof packet, packet);
			return;
		}
	} while(!item.SHIFT);
}

// Handle the keyboard: BBC VDU Mode
//
void do_keyboard() {
  	fabgl::Keyboard* kb = PS2Controller.keyboard();
	fabgl::VirtualKeyItem item;

	#if SERIALKB == 1
	if(DBGSerial.available()) {
		byte packet[] = {
			DBGSerial.read(),
			0,
		};
		send_packet(PACKET_KEYCODE, sizeof packet, packet);
		return;
	}
	#endif
	
	if(kb->getNextVirtualKey(&item, 0)) {
		if(item.down) {
			switch(item.vk) {
				case fabgl::VK_LEFT:
					keycode = 0x08;
					break;
				case fabgl::VK_TAB:
					keycode = 0x09;
					break;
				case fabgl::VK_RIGHT:
					keycode = 0x15;
					break;
				case fabgl::VK_DOWN:
					keycode = 0x0A;
					break;
				case fabgl::VK_UP:
					keycode = 0x0B;
					break;
				case fabgl::VK_BACKSPACE:
					keycode = 0x7F;
					break;
				default:
					keycode = item.ASCII;	
					break;
			}
			// Pack the modifiers into a byte
			//
			modifiers = 
				item.CTRL		<< 0 |
				item.SHIFT		<< 1 |
				item.LALT		<< 2 |
				item.RALT		<< 3 |
				item.CAPSLOCK	<< 4 |
				item.NUMLOCK	<< 5 |
				item.SCROLLLOCK << 6 |
				item.GUI		<< 7
			;
		}
		// Handle some control keys
		//
		switch(keycode) {
			case 14: setPagedMode(true); break;
			case 15: setPagedMode(false); break;
		}
		// Create and send the packet back to MOS
		//
		byte packet[] = {
			keycode,
			modifiers,
			item.vk,
			item.down,
		};
		send_packet(PACKET_KEYCODE, sizeof packet, packet);
	}
}

// Sprite Engine
//
void vdu_sys_sprites(void) {
    uint32_t	color;
    void *		dataptr;
    int16_t 	x, y;
    int16_t 	width, height;
    uint16_t 	n, temp;
    
    int cmd = readByte_t();

    switch(cmd) {
    	case 0: {	// Select bitmap
			int	rb = readByte_t();
			if(rb >= 0) {
        		current_bitmap = rb;
        		debug_log("vdu_sys_sprites: bitmap %d selected\n\r", current_bitmap);
			}
		}	break;
      	
		case 1: 	// Send bitmap data
      	case 2: {	// Define bitmap in single color
			int rw = readWord_t(); if(rw == -1) return;
			int rh = readWord_t(); if(rh == -1) return;

			width = rw;
			height = rh;
			//
        	// Clear out any old data first
			//
        	free(bitmaps[current_bitmap].data);
			//
        	// Allocate new heap data
			//
        	dataptr = (void *)heap_caps_malloc(sizeof(uint32_t)*width*height, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        	bitmaps[current_bitmap].data = (uint8_t *)dataptr;

        	if(dataptr != NULL) {                  
				if(cmd == 1) {
					//
					// Read data to the new databuffer
					//
					for(n = 0; n < width*height; n++) ((uint32_t *)bitmaps[current_bitmap].data)[n] = readLong_b();  
					debug_log("vdu_sys_sprites: bitmap %d - data received - width %d, height %d\n\r", current_bitmap, width, height);
				}
				if(cmd == 2) {
					color = readLong_b();
					//
					// Define single color
					//
					for(n = 0; n < width*height; n++) ((uint32_t *)dataptr)[n] = color;            
					debug_log("vdu_sys_sprites: bitmap %d - set to solid color - width %d, height %d\n\r", current_bitmap, width, height);            
				}
				// Create bitmap structure
				//
				bitmaps[current_bitmap] = Bitmap(width,height,dataptr,PixelFormat::RGBA8888);
				bitmaps[current_bitmap].dataAllocated = false;
			}
	        else { 
				//
				// Discard incoming serial data if failed to allocate memory
				//
				if (cmd == 1) {
					discardBytes(4*width*height);
				}
				if (cmd == 2) {
					discardBytes(4);
				}
				debug_log("vdu_sys_sprites: bitmap %d - data discarded, no memory available - width %d, height %d\n\r", current_bitmap, width, height);
			}
		}	break;
      	
		case 3: {	// Draw bitmap to screen (x,y)
			int	rx = readWord_t(); if(rx == -1) return; x = rx;
			int ry = readWord_t(); if(ry == -1) return; y = ry;

			if(bitmaps[current_bitmap].data) {
				Canvas->drawBitmap(x,y,&bitmaps[current_bitmap]);
				doWaitCompletion = true;
			}
			debug_log("vdu_sys_sprites: bitmap %d draw command\n\r", current_bitmap);
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
			int b = readByte_t(); if(b == -1) return;
			current_sprite = b;
			debug_log("vdu_sys_sprites: sprite %d selected\n\r", current_sprite);
		}	break;

      	case 5: {	// Clear frames
			sprites[current_sprite].visible = false;
			sprites[current_sprite].setFrame(0);
			sprites[current_sprite].clearBitmaps();
			debug_log("vdu_sys_sprites: sprite %d - all frames cleared\n\r", current_sprite);
		}	break;
        
      	case 6:	{	// Add frame to sprite
			int b = readByte_t(); if(b == -1) return; n = b;
			sprites[current_sprite].addBitmap(&bitmaps[n]);
			debug_log("vdu_sys_sprites: sprite %d - bitmap %d added as frame %d\n\r", current_sprite, n, sprites[current_sprite].framesCount-1);
		}	break;

      	case 7:	{	// Active sprites to GDU
			int b = readByte_t(); if(b == -1) return;
			/*
			* Sprites 0-(numsprites-1) will be activated on-screen
			* Make sure all sprites have at least one frame attached to them
			*/
			numsprites = b;
			if(numsprites) {
				VGAController->setSprites(sprites, numsprites);
			}
			else {
				VGAController->removeSprites();
			}
			doWaitCompletion = true;
			debug_log("vdu_sys_sprites: %d sprites activated\n\r", numsprites);
		}	break;

      	case 8: {	// Set next frame on sprite
			sprites[current_sprite].nextFrame();
			debug_log("vdu_sys_sprites: sprite %d next frame\n\r", current_sprite);
		}	break;

      	case 9:	{	// Set previous frame on sprite
			n = sprites[current_sprite].currentFrame;
			if(n) sprites[current_sprite].currentFrame = n-1; // previous frame
			else sprites[current_sprite].currentFrame = sprites[current_sprite].framesCount - 1;  // last frame
			debug_log("vdu_sys_sprites: sprite %d previous frame\n\r", current_sprite);
		}	break;

      	case 10: {	// Set current frame id on sprite
			int b = readByte_t(); if(b == -1) return;
			n = b;
			if(n < sprites[current_sprite].framesCount) sprites[current_sprite].currentFrame = n;
			debug_log("vdu_sys_sprites: sprite %d set to frame %d\n\r", current_sprite,n);
		}	break;

      	case 11: {	// Show sprite
        	sprites[current_sprite].visible = 1;
			debug_log("vdu_sys_sprites: sprite %d show cmd\n\r", current_sprite);
		}	break;

      	case 12: {	// Hide sprite
			sprites[current_sprite].visible = 0;
			debug_log("vdu_sys_sprites: sprite %d hide cmd\n\r", current_sprite);
		}	break;

      	case 13: {	// Move sprite to coordinate on screen
			int	rx = readWord_t(); if(rx == -1) return; x = rx;
			int ry = readWord_t(); if(ry == -1) return; y = ry;

			sprites[current_sprite].moveTo(x, y); 
			debug_log("vdu_sys_sprites: sprite %d - move to (%d,%d)\n\r", current_sprite, x, y);
		}	break;

      	case 14: {	// Move sprite by offset to current coordinate on screen
			int	rx = readWord_t(); if(rx == -1) return; x = rx;
			int ry = readWord_t(); if(ry == -1) return; y = ry;
			sprites[current_sprite].x += x;
			sprites[current_sprite].y += y;
			debug_log("vdu_sys_sprites: sprite %d - move by offset (%d,%d)\n\r", current_sprite, x, y);
		}	break;

	  	case 15: {	// Refresh
			if(numsprites) { 
				VGAController->refreshSprites();
			}
			debug_log("vdu_sys_sprites: perform sprite refresh\n\r");
		}	break;
		case 16: {	// Reset
			cls(false);
			for(n = 0; n < MAX_SPRITES; n++) {
				sprites[n].visible = false;
				sprites[current_sprite].setFrame(0);
				sprites[n].clearBitmaps();
			}
			for(n = 0; n < MAX_BITMAPS; n++) {
	        	free(bitmaps[n].data);
				bitmaps[n].dataAllocated = false;
			}
			doWaitCompletion = true;
			debug_log("vdu_sys_sprites: reset\n\r");
		}	break;
    }
}

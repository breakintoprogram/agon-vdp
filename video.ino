//
// Title:	        Agon Video BIOS
// Author:        	Dean Belfield
// Contributors:	Jeroen Venema (Sprite Code)
//					Damien Guard (Fonts)
// Created:       	22/03/2022
// Last Updated:	02/10/2022
//
// Modinfo:
// 11/07/2022:		Baud rate tweaked for Agon Light, HW Flow Control temporarily commented out
// 26/07/2022:		Added VDU 29 support
// 03/08/2022:		Set codepage 1252, fixed keyboard mappings for AGON, added cursorTab, VDP serial protocol
// 06/08/2022:		Added a custom font, fixed UART initialisation, flow control
// 10/08/2022:		Improved keyboard mappings, added sprites, audio, new font
// 05/09/2022:		Moved the audio class to agon_audio.h, added function prototypes in agon.h
// 02/10/2022:		Version 1.00: Tweaked the sprite code, changed bootup title to Quark

#include "fabgl.h"
#include "HardwareSerial.h"

#define VERSION			1
#define REVISION		0

#define	DEBUG			0		// Serial Debug Mode: 1 = enable

fabgl::PS2Controller		PS2Controller;
fabgl::VGA16Controller		VGAController;
fabgl::Canvas *				Canvas;
fabgl::SoundGenerator		SoundGenerator;

#include "agon.h"
#include "agon_fonts.h"			// The Acorn BBC Micro Font
#include "agon_audio.h"			// The Audio class

int         charX, charY;

Point		origin;
Point       p1, p2, p3;
RGB888      gfg;
RGB888      tfg, tbg;

int         count = 0;

audio_channel *	audio_channels[AUDIO_CHANNELS];

// Sprite data
//
uint8_t	numsprites = 0;
uint8_t current_sprite = 0; 
uint8_t current_bitmap = 0;
Bitmap	bitmaps[256];
Sprite	sprites[256];

#if DEBUG == 1
HardwareSerial DBGSerial(0);
#endif 

void setup() {
	disableCore0WDT(); delay(200);								// Disable the watchdog timers
	disableCore1WDT(); delay(200);
	#if DEBUG == 1
	DBGSerial.begin(500000, SERIAL_8N1, 3, 1);
	#endif 
	ESPSerial.end();
 	ESPSerial.setRxBufferSize(UART_RX_SIZE);					// Can't be called when running
 	ESPSerial.begin(UART_BR, SERIAL_8N1, UART_RX, UART_TX);
	#if USE_HWFLOW == 1
	ESPSerial.setHwFlowCtrlMode(HW_FLOWCTRL_RTS, 64);			// Can be called whenever
	ESPSerial.setPins(UART_NA, UART_NA, UART_CTS, UART_RTS);	// Must be called after begin
	#else 
	pinMode(UART_RTS, OUTPUT);
	pinMode(UART_CTS, INPUT);	
	setRTSStatus(true);
	#endif
 	PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
	PS2Controller.keyboard()->setLayout(&fabgl::UKLayout);
  	VGAController.begin();
	init_audio();
	copy_font();
  	set_mode(5);
	boot_screen();
}

// Copy the AGON font data from Flash to RAM
//
void copy_font() {
	memcpy(fabgl::FONT_AGON_DATA + 256, fabgl::FONT_AGON_BITMAP, sizeof(fabgl::FONT_AGON_BITMAP));
}

// Set the RTS line value
//
void setRTSStatus(bool value) {
	digitalWrite(UART_RTS, value ? LOW : HIGH);		// Asserts when LOW
}

// Initialise the sound driver
//
void init_audio() {
	for(int i = 0; i < AUDIO_CHANNELS; i++) {
		init_audio_channel(i);
	}
}
void init_audio_channel(int channel) {
  	xTaskCreatePinnedToCore(audio_driver,  "audio_driver",
    	4096,					// This stack size can be checked & adjusted by reading the Stack Highwater
        &channel,				// Parameters
        PLAY_SOUND_PRIORITY,	// Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
        NULL,
    	ARDUINO_RUNNING_CORE
	);
}

// The music driver task
//
void audio_driver(void * parameters) {
	int channel = *(int *)parameters;

	audio_channels[channel] = new audio_channel(channel);
	while(true) {
		audio_channels[channel]->loop();
		vTaskDelay(1);
	}
}

// The main loop
//
void loop() {
	bool cursorVisible = false;
	bool cursorState = false;

	while(true) {
    	cursorVisible = ((count & 0xFFFF) == 0);
    	if(cursorVisible) {
      		cursorState = !cursorState;
      		do_cursor();
    	}
    	do_keyboard();
    	if(ESPSerial.available() > 0) {
			#if USE_HWFLOW == 0
			if(ESPSerial.available() > UART_RX_THRESH) {
				setRTSStatus(false);		
			}
			#endif 
      		if(cursorState) {
 	    		cursorState = false;
        		do_cursor();
      		}
      		byte c = ESPSerial.read();
      		vdu(c);
    	}
		#if USE_HWFLOW == 0
		else {
			setRTSStatus(true);
		}
		#endif
    	count++;
  	}
}
	
// Send the cursor position back to MOS
//
void sendCursorPosition() {
	byte packet[] = {
		charX / Canvas->getFontInfo()->width,
		charY / Canvas->getFontInfo()->height,
	};
	send_packet(PACKET_CURSOR, sizeof packet, packet);	
}

// Send a character back to MOS
//
void sendScreenChar(int x, int y) {
	int	px = x * Canvas->getFontInfo()->width;
	int py = y * Canvas->getFontInfo()->height;
	char c = get_screen_char(px, py);
	byte packet[] = {
		c,
	};
	send_packet(PACKET_SCRCHAR, sizeof packet, packet);
}

// Send a pixel value back to MOS
//
void sendScreenPixel(int x, int y) {
	RGB888 pixel;

	// Do some bounds checking first
	//
	if(x >= 0 && y >= 0 && x < Canvas->getWidth() && y < Canvas->getHeight()) {
		pixel = Canvas->getPixel(x, y);
	}	
	byte packet[] = {
		pixel.R,
		pixel.G,
		pixel.B,
	};
	send_packet(PACKET_SCRPIXEL, sizeof packet, packet);	
}

// Send an audio acknowledgement
//
void sendPlayNote(int channel, int success) {
	byte packet[] = {
		channel,
		success,
	};
	send_packet(PACKET_AUDIO, sizeof packet, packet);	
}

// Send MODE information (screen details)
//
void sendModeInformation() {
	int w = Canvas->getWidth();
	int h = Canvas->getHeight();
	byte packet[] = {
		w & 0xFF,	 						// Width in pixels (L)
		(w >> 8) & 0xFF,					// Width in pixels (H)
		h & 0xFF,							// Height in pixels (L)
		(h >> 8) & 0xFF,					// Height in pixels (H)
		w / Canvas->getFontInfo()->width ,	// Width in characters (byte)
		h / Canvas->getFontInfo()->height ,	// Height in characters (byte)
	};
	send_packet(PACKET_MODE, sizeof packet, packet);
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

// The boot screen
//
void boot_screen() {
  	cls();
  	printFmt("Agon Quark VPD Version %d.%02d\n\r", VERSION, REVISION);
	ESPSerial.write(27);	// The MOS will wait for this escape character during initialisation
}

// Clear the screen
// 
void cls() {
//	Canvas->swapBuffers();
	Canvas->clear();
	charX = 0;
	charY = 0;
}

// Clear the graphics area
//
void clg() {
	Canvas->clear();
}

// Try and match a character
//
char get_screen_char(int px, int py) {
	RGB888	pixel;
	uint8_t	charRow;
	uint8_t	charData[8];

	// Do some bounds checking first
	//
	if(px < 0 || py < 0 || px >= Canvas->getWidth() - 8 || py >= Canvas->getHeight() - 8) {
		return 0;
	}

	// Now scan the screen and get the 8 byte pixel representation in charData
	//
	for(int y = 0; y < 8; y++) {
		charRow = 0;
		for(int x = 0; x < 8; x++) {
			pixel = Canvas->getPixel(px + x, py + y);
			if(pixel == tfg) {
				charRow |= (0x80 >> x);
			}
		}
		charData[y] = charRow;
	}
	//
	// Finally try and match with the character set array
	//
	for(int i = 32; i < 128; i++) {
		if(cmp_char(charData, &fabgl::FONT_AGON_DATA[i * 8], 8)) {	
			return i;		
		}
	}
	return 0;
}

bool cmp_char(uint8_t * c1, uint8_t *c2, int len) {
	for(int i = 0; i < len; i++) {
		if(*c1++ != *c2++) {
			return false;
		}
	}
	return true;
}

// Set the video mode
// 
void set_mode(int mode) {
	switch(mode) {
		case 0:
			VGAController.setResolution(SVGA_1024x768_75Hz);
			break;
		case 1:
			VGAController.setResolution(SVGA_800x600_56Hz);
			break;
		case 2:
      		VGAController.setResolution(VGA_640x480_60Hz);
      		break;
		case 3:
			VGAController.setResolution(VGA_400x300_60Hz);
			break;
    	case 4:
      		VGAController.setResolution(VGA_320x200_75Hz);
      		break;
		case 5:
		    VGAController.setResolution(VGA_512x384_60Hz);
      		break;
		case 7:
			VGAController.setResolution(PAL_720x576_50Hz);
			break;
  	}
  	delete Canvas;

  	Canvas = new fabgl::Canvas(&VGAController);
  	gfg = Color::BrightWhite;
	tfg = Color::BrightWhite;
	tbg = Color::Black;
  	Canvas->selectFont(&fabgl::FONT_AGON);
  	Canvas->setGlyphOptions(GlyphOptions().FillBackground(true));
  	Canvas->setPenWidth(1);
  	Canvas->setPenColor(tfg);
  	Canvas->setBrushColor(tbg);	
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

// Handle the keyboard
// 
void do_keyboard() {
  	fabgl::Keyboard* kb = PS2Controller.keyboard();
	fabgl::VirtualKeyItem item;
	byte keycode;
	byte modifiers;

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
				case fabgl::VK_POUND:
					keycode = '`';
					break;
				case fabgl::VK_BACKSPACE:
					keycode = 0x7F;
					break;
				case fabgl::VK_GRAVEACCENT:
					keycode = 0x00;
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
			// Create and send the packet back to MOS
			//
			byte packet[] = {
				keycode,
				modifiers,
			};
			send_packet(PACKET_KEYCODE, sizeof packet, packet);
		}
	}
}

// Send a packet of data to the MOS
//
void send_packet(byte code, byte len, byte data[]) {
	ESPSerial.write(code + 0x80);
	ESPSerial.write(len);
	for(int i = 0; i < len; i++) {
		ESPSerial.write(data[i]);
	}
}

// Render a cursor at the current screen position
//
void do_cursor() {
  	int w = Canvas->getFontInfo()->width;
  	int h = Canvas->getFontInfo()->height;
  	int x = charX; 
  	int y = charY;
  	Canvas->swapRectangle(x, y, x + w - 1, y + h - 1);
}

// Read an unsigned byte from the serial port
//
byte readByte() {
  	while(ESPSerial.available() == 0);
  	return ESPSerial.read();
}

// Read an unsigned word from the serial port
//
word readWord() {
  	byte l = readByte();
  	byte h = readByte();
  	return (h << 8) | l;
}

// Read an unsigned word from the serial port
//
uint32_t readLong() {
  uint32_t temp;
  temp  =  readByte();			// LSB;
  temp |= (readByte() << 8);
  temp |= (readByte() << 16);
  temp |= (readByte() << 24);
  return temp;
}

// Translate a point
//
Point translate(Point p) {
	return translate(p.X, p.Y);
}
Point translate(int X, int Y) {
	return Point(origin.X + X, origin.Y + Y);
}

void vdu(byte c) {
	if(c >= 0x20 && c != 0x7F) {
		Canvas->setPenColor(tfg);
		Canvas->setBrushColor(tbg);
  		Canvas->drawTextFmt(charX, charY, "%c", c);
   		cursorRight();
	}
	else {
		switch(c) {
			case 0x08:  // Cursor Left
				cursorLeft();
				break;
			case 0x09:  // Cursor Right
				cursorRight();
				break;
			case 0x0A:  // Cursor Down
				cursorDown();
				break;
			case 0x0B:  // Cursor Up
				cursorUp();
				break;
			case 0x0C:  // CLS
				cls();
				break;
			case 0x0D:  // CR
				cursorHome();
				break;
			case 0x10:	// CLG
				clg();
				break;
			case 0x11:	// COLOUR
				vdu_colour();
				break;
			case 0x12:  // GCOL
				vdu_gcol();
				break;
			case 0x16:  // Mode
				vdu_mode();
				break;
			case 0x17:  // VDU 23
				vdu_sys();
				break;
			case 0x19:  // PLOT
				vdu_plot();
				break;
			case 0x1D:	// VDU_29
				vdu_origin();
			case 0x1E:  // Home
				cursorHome();
				break;
			case 0x1F:	// TAB(X,Y)
				cursorTab();
				break;
			case 0x7F:  // Backspace
				cursorLeft();
				Canvas->drawText(charX, charY, " ");
				break;
		}
	}
}

// Handle the cursor
//
void cursorLeft() {
	charX -= Canvas->getFontInfo()->width;
  	if(charX < 0) {
    	charX = 0;
  	}
}

void cursorRight() {
  	charX += Canvas->getFontInfo()->width;
  	if(charX >= Canvas->getWidth()) {
    	cursorDown();
    	cursorHome();
  	}
}

void cursorDown() {
	int h = Canvas->getFontInfo()->height;
  	charY += h;
	  if(charY >= Canvas->getHeight()) {
		charY -= h;
		Canvas->scroll(0, -h);
	  }
}

void cursorUp() {
  	charY -= Canvas->getFontInfo()->height;
  	if(charY < 0) {
		charY = 0;
  	}
}

void cursorHome() {
  	charX = 0;
}

void cursorTab() {
	charX = readByte() * Canvas->getFontInfo()->width;
	charY = readByte() * Canvas->getFontInfo()->height;
}

// Handle MODE
//
void vdu_mode() {
  	byte mode = readByte();
	debug_log("vdu_mode: %d\n\r", mode);
  	set_mode(mode);
}

// Handle VDU 29
//
void vdu_origin() {
	origin.X = readWord();
	origin.Y = readWord();
	debug_log("vdu_origin: %d,%d\n\r", origin.X, origin.Y);
}

// Handle COLOUR
// 
void vdu_colour() {
  	byte mode = readByte();
	if(mode == 0) {
  		tfg.R = readByte();
  		tfg.G = readByte();
  		tfg.B = readByte();
	}
	else {
  		tbg.R = readByte();
  		tbg.G = readByte();
  		tbg.B = readByte();
	}
	debug_log("vdu_colour: %d,%d,%d,%d\n\r", mode, gfg.R, gfg.G, gfg.B);
}

// Handle GCOL
// 
void vdu_gcol() {
  	byte mode = readByte();
  	gfg.R = readByte();
  	gfg.G = readByte();
  	gfg.B = readByte();
	debug_log("vdu_gcol: %d,%d,%d,%d\n\r", mode, gfg.R, gfg.G, gfg.B);
}

// Handle PLOT
//
void vdu_plot() {
  	byte mode = readByte();
  	p3 = p2;
  	p2 = p1;
  	p1.X = readWord();
  	p1.Y = readWord();
	Canvas->setPenColor(gfg);
	debug_log("vdu_plot: %d,%d,%d\n\r", mode, p1.X, p1.Y);
  	switch(mode) {
    	case 0x04: 
      		Canvas->moveTo(origin.X + p1.X, origin.Y + p1.Y);
      		break;
    	case 0x05: // Line
      		Canvas->lineTo(origin.X + p1.X, origin.Y + p1.Y);
      		break;
		case 0x40 ... 0x47: // Point
			vdu_plot_point(mode);
			break;
    	case 0x50 ... 0x57: // Triangle
      		vdu_plot_triangle(mode);
      		break;
    	case 0x90 ... 0x97: // Circle
      		vdu_plot_circle(mode);
      		break;
  	}
}

void vdu_plot_point(byte mode) {
	Canvas->setPixel(origin.X + p1.X, origin.Y + p1.Y);
}

void vdu_plot_triangle(byte mode) {
  	Point p[3] = { 
		translate(p3),
		translate(p2),
		translate(p1), 
	};
  	Canvas->setBrushColor(gfg);
  	Canvas->fillPath(p, 3);
  	Canvas->setBrushColor(tbg);
}

void vdu_plot_circle(byte mode) {
  	int a, b, r;
  	switch(mode) {
    	case 0x90 ... 0x93: // Circle
      		r = 2 * (p1.X + p1.Y);
      		Canvas->drawEllipse(origin.X + p2.X, origin.Y + p2.Y, r, r);
      		break;
    	case 0x94 ... 0x97: // Circle
      		a = p2.X - p1.X;
      		b = p2.Y - p1.Y;
      		r = 2 * sqrt(a * a + b * b);
      		Canvas->drawEllipse(origin.X + p2.X, origin.Y + p2.Y, r, r);
      		break;
  	}
}

// Handle SYS
// VDU 23,mode
//
void vdu_sys() {
  	byte mode = readByte();
	debug_log("vdu_sys: %d\n\r", mode);
	//
	// If mode < 32, then it's a system command
	//
	if(mode < 32) {
  		switch(mode) {
		    case 0x00:				// VDU 23, 0
      			vdu_sys_video();	// Video system control
      			break;
			case 0x1B:				// VDU 23, 27
				vdu_sys_sprites();	// Sprite system control
				break;
  		}
	}
	//
	// Otherwise, do
	// VDU 23,mode,n1,n2,n3,n4,n5,n6,n7,n8
	// Redefine character with ASCII code mode
	//
	else {
		uint8_t * ptr = &fabgl::FONT_AGON_DATA[mode * 8];
		for(int i = 0; i < 8; i++) {
			*ptr++ = readByte();
		}	
	}
}

// VDU 23,0: Video system control
// These send responses back and have a packet # that matches the VDU command
//
void vdu_sys_video() {
  	byte mode = readByte();
  	switch(mode) {
		case PACKET_CURSOR: {		// VDU 23, 0, 2
			sendCursorPosition();	// Send cursor position
		}	break;
		case PACKET_SCRCHAR: {		// VDU 23, 0, 3, x; y;
			word x = readWord();	// Get character at screen position x, y
			word y = readWord();
			sendScreenChar(x, y);
		}	break;
		case PACKET_SCRPIXEL: {		// VDU 23, 0, 4, x; y;
			word x = readWord();	// Get pixel value at screen position x, y
			word y = readWord();
			sendScreenPixel(x, y);
		} 	break;		
		case PACKET_AUDIO: {		// VDU 23, 0, 5, channel, waveform, volume, freq; duration;
			byte channel = readByte();
			byte waveform = readByte();
			byte volume = readByte();
			word frequency = readWord();
			word duration = readWord();
			word success = play_note(channel, volume, frequency, duration);
			sendPlayNote(channel, success);
		}	break;
		case PACKET_MODE: {			// VDU 23, 0, 6
			sendModeInformation();	// Send mode information (screen dimensions, etc)
		}	break;
  	}
}

// Play a note
//
word play_note(byte channel, byte volume, word frequency, word duration) {
	if(channel >=0 && channel < AUDIO_CHANNELS) {
		return audio_channels[channel]->play_note(volume, frequency, duration);
	}
	return 0;
}

// Sprite Engine
//
void vdu_sys_sprites(void) {
    uint32_t	color;
    void *		dataptr;
    int16_t 	x, y;
    int16_t 	width, height;
    uint16_t 	n, temp;
    
    byte cmd = readByte();

    switch(cmd) {
    	case 0: // Select bitmap
        	current_bitmap = readByte();
        	debug_log("vdu - bitmap %d selected\n\r", current_bitmap);
        	break;
      	case 1: // Send bitmap data
      	case 2: // Define bitmap in single color
        	width = readWord();
        	height = readWord();
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
					for(n = 0; n < width*height; n++) ((uint32_t *)bitmaps[current_bitmap].data)[n] = readLong();  
					debug_log("vdu - bitmap %d - data received - width %d, height %d\n\r", current_bitmap, width, height);
				}
				if(cmd == 2) {
					color = readLong();
					// Define single color
					//
					for(n = 0; n < width*height; n++) ((uint32_t *)dataptr)[n] = color;            
					debug_log("vdu - bitmap %d - set to solid color - width %d, height %d\n\r", current_bitmap, width, height);            
				}
				// Create bitmap structure
				//
				bitmaps[current_bitmap] = Bitmap(width,height,dataptr,PixelFormat::RGBA8888);
				bitmaps[current_bitmap].dataAllocated = false;
			}
	        else {
    	    	for(n = 0; n < width*height; n++) readLong(); // discard incoming data
        		debug_log("vdu - bitmap %d - data discarded, no memory available - width %d, height %d\n\r", current_bitmap, width, height);
        	}
        	break;

      	case 3: // Draw bitmap to screen (x,y)
			x = readWord();
			y = readWord();

			if(bitmaps[current_bitmap].data) Canvas->drawBitmap(x,y,&bitmaps[current_bitmap]);
	
			debug_log("vdu - bitmap %d draw command\n\r", current_bitmap);
        break;

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
    	case 4: // Select sprite
			current_sprite = readByte();
			debug_log("vdu - sprite %d selected\n\r", current_sprite);
			break;

      	case 5: // Clear frames
			sprites[current_sprite].clearBitmaps();
			debug_log("vdu - sprite %d - all frames cleared\n\r", current_sprite);
			break;
        
      case 6: // Add frame to sprite
			n = readByte();
			sprites[current_sprite].addBitmap(&bitmaps[n]);
			sprites[current_sprite].visible = false;
			debug_log("vdu - sprite %d - bitmap %d added as frame %d\n\r", current_sprite, n, sprites[current_sprite].framesCount-1);
			break;

      case 7: // Active sprites to GDU
			/*
			* Sprites 0-(numsprites-1) will be activated on-screen
			* Make sure all sprites have at least one frame attached to them
			*/
			numsprites = readByte();
			VGAController.setSprites(sprites, numsprites);
			debug_log("vdu - %d sprites activated\n\r", numsprites);
			break;

      case 8: // set next frame on sprite
			sprites[current_sprite].nextFrame();
			debug_log("vdu - sprite %d next frame\n\r", current_sprite);
			break;

      case 9: // set previous frame on sprite
			n = sprites[current_sprite].currentFrame;
			if(n) sprites[current_sprite].currentFrame = n-1; // previous frame
			else sprites[current_sprite].currentFrame = sprites[current_sprite].framesCount - 1;  // last frame
			debug_log("vdu - sprite %d previous frame\n\r", current_sprite);
			break;

      case 10: // set current frame id on sprite
			n = readByte();
			if(n < sprites[current_sprite].framesCount) sprites[current_sprite].currentFrame = n;
			debug_log("vdu - sprite %d set to frame %d\n\r", current_sprite,n);
			break;

      case 11: // Show sprite
        	sprites[current_sprite].visible = 1;
			debug_log("vdu - sprite %d show cmd\n\r", current_sprite);
			break;

      case 12: // Hide sprite
			sprites[current_sprite].visible = 0;
			debug_log("vdu - sprite %d hide cmd\n\r", current_sprite);
			break;

      case 13: // Move sprite to coordinate on screen
			x = readWord();
			y = readWord();	
			sprites[current_sprite].moveTo(x,y); 
			debug_log("vdu - sprite %d - move to (%d,%d)\n\r", current_sprite, x, y);
			break;

      case 14: // move sprite by offset to current coordinate on screen
			x = readWord();
			y = readWord();
			sprites[current_sprite].x += x;
			sprites[current_sprite].y += y;
			debug_log("vdu - sprite %d - move by offset (%d,%d)\n\r", current_sprite, x, y);
			break;

	  case 15: // Refresh
			if(numsprites) { 
				VGAController.refreshSprites();
			}
			debug_log("vdu - perform sprite refresh\n\r");
			break;
    }
}

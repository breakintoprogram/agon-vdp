
//
// Title:	        Agon Video BIOS
// Author:        	Dean Belfield
// Created:       	22/03/2022
// Last Updated:	06/08/2022
//
// Modinfo:
// 11/07/2022:		Baud rate tweaked for Agon Light, HW Flow Control temporarily commented out
// 26/07/2022:		Added VDU 29 support
// 03/08/2022:		Set codepage 1252, fixed keyboard mappings for AGON, added cursorTab, VDP serial protocol
// 06/08/2022:		Added a custom font, fixed UART initialisation, flow control

#include "fabgl.h"
#include "HardwareSerial.h"

#include "font_agon.h"

#define VERSION			0
#define REVISION		4

#define	DEBUG			0		// 1 = enable debug mode
#define USE_HWFLOW		1		// 1 = enable hardware flow control 

#define	ESPSerial Serial2

#define UART_BR			384000
#define UART_NA			-1
#define UART_TX			2
#define UART_RX			34
#define UART_RTS  	  	13 		// The ESP32 RTS pin (eZ80 CTS)
#define UART_CTS  	  	14		// The ESP32 CTS pin (eZ80 RTS)

#define UART_RX_SIZE	256		// The RX buffer size
#define UART_RX_THRESH	128		// Point at which RTS is toggled

#define GPIO_ITRP		17		// VSync Interrupt Pin - for reference only

#define PACKET_GP		0		// General poll data
#define PACKET_KEYCODE	1		// Keyboard data
#define PACKET_CURSOR	2		// Cursor positions

fabgl::PS2Controller    PS2Controller;
fabgl::VGA16Controller  VGAController;
fabgl::SoundGenerator	SoundGenerator;

fabgl::Canvas * Canvas;

int         charX, charY;

Point		origin;
Point       p1, p2, p3;
RGB888      gfg;
RGB888      tfg, tbg;

int         count = 0;

#if DEBUG == 1
HardwareSerial DBGSerial(0);
#endif 

void setup() {
	disableCore0WDT();				// Disable the watchdog timers
	delay(100); 					// Crashes without this delay
	disableCore1WDT();
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
	PS2Controller.keyboard()->setCodePage(fabgl::CodePages::get(1252));
  	VGAController.begin();
	copy_font();
  	set_mode(5);
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

// The main loop
//
void loop() {
	bool cursorVisible = false;
	bool cursorState = false;

	boot_screen();

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
			sendCursorPosition();
    	}
		#if USE_HWFLOW == 0
		else {
			setRTSStatus(true);
		}
		#endif
    	count++;
  	}
}

void sendCursorPosition() {
	// Create and send the packet back to MOS
	//
	byte packet[] = {
		charX / Canvas->getFontInfo()->width,
		charY / Canvas->getFontInfo()->height,
	};
	send_packet(PACKET_CURSOR, sizeof packet, packet);	
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
  	printFmt("AGON VPD Version %d.%02d\n\r", VERSION, REVISION);
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
				case fabgl::VK_BACKSPACE:
					keycode = 0x7F;
					break;
				case fabgl::VK_AT:
					keycode = '"';
					break;
				case fabgl::VK_QUOTEDBL:
					keycode = '@';
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
			case 0x1C:				// VDU 28, 28
				vdu_sys_audio();	// Audio system control
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
//
void vdu_sys_video() {
  	byte mode = readByte();
  	switch(mode) {
  	}
}

// VDU 23, 28: Audio system control
//
void vdu_sys_audio() {
	byte mode = readByte();
	switch(mode) {
		case 0:						// VDU 23, 28, 0, volume, frequency; duration;
			play_sound();
			break;
	}
}

// Play a sound
//
void play_sound(void) {
	int volume = readByte();
	int frequency = readWord();
	int duration = readWord();
	SoundGenerator.playSound(SineWaveformGenerator(), frequency, duration, volume);
}


//
// Title:	        Agon Video BIOS
// Author:        	Dean Belfield
// Created:       	22/03/2022
// Last Updated:	03/08/2022
//
// Modinfo:
// 11/07/2022:		Baud rate tweaked for Agon Light, HW Flow Control temporarily commented out
// 26/07/2022:		Added VDU 29 support
// 03/08/2022:		Set codepage 1252, fixed keyboard mappings for AGON, added cursorTab, VDP serial protocol

#include "fabgl.h"
#include "HardwareSerial.h"

#define VERSION		0
#define REVISION	3

#define	DEBUG		0

#define	ESPSerial Serial2

#define UART_BR		384000
#define UART_NA		-1
#define UART_TX		2
#define UART_RX		34
#define UART_RTS    13 		// The ESP32 RTS pin
#define UART_CTS    14		// The ESP32 CTS pin

#define GPIO_ITRP	17		// VSync Interrupt Pin - for reference only

fabgl::PS2Controller    PS2Controller;
fabgl::VGA16Controller  VGAController;

fabgl::Canvas * Canvas;

int         charX, charY;

Point		origin;
Point       p1, p2, p3;
RGB888      fg, bg;

int         count = 0;

#if DEBUG == 1
HardwareSerial DBGSerial(0);
#endif 

void setup() {
	disableCore0WDT();			// Disable the watchdog timers
	delay(100); 				// Crashes without this delay
	disableCore1WDT();
//	pinMode(UART_RTS, OUTPUT);	// This should be done by setPins
//	pinMode(UART_CTS, INPUT);
	#if DEBUG == 1
	DBGSerial.begin(500000, SERIAL_8N1, 3, 1);
	#endif 
 	ESPSerial.begin(UART_BR, SERIAL_8N1, UART_RX, UART_TX);
//	ESPSerial.setPins(UART_NA, UART_NA, UART_CTS, UART_RTS);
//	ESPSerial.setHwFlowCtrlMode(HW_FLOWCTRL_CTS_RTS, 64);
 	ESPSerial.setRxBufferSize(1024);
//	setRTSStatus(true);
 	PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
	PS2Controller.keyboard()->setCodePage(fabgl::CodePages::get(1252));
  	VGAController.begin();
  	set_mode(6);
}

void setRTSStatus(bool value) {
	digitalWrite(UART_RTS, value ? LOW : HIGH);		// Asserts when low
}

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
      		if(cursorState) {
 	    		cursorState = false;
        		do_cursor();
      		}
      		byte c = ESPSerial.read();
      		vdu(c);
			
    	}
    	count++;
  	}
}

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

void boot_screen() {
  	cls();
  	printFmt("AGON VPD Version %d.%02d\n\r", VERSION, REVISION);
	ESPSerial.write(27);	// The MOS will wait for this escape character during initialisation
}

void cls() {
//	Canvas->swapBuffers();
	Canvas->clear();
	charX = 0;
	charY = 0;
}

// Set the video mode
// 
void set_mode(int mode) {
	switch(mode) {
    	case 2:
      		VGAController.setResolution(VGA_320x200_75Hz, 320, 200, false);
      		break;
		case 6:
      		VGAController.setResolution(VGA_640x480_60Hz, 640, 400, false);
      		break;
    	default:
			VGAController.setResolution(VGA_640x480_60Hz);
      		break;
  	}
  	delete Canvas;

  	Canvas = new fabgl::Canvas(&VGAController);
  	fg = Color::BrightWhite;
  	bg = Color::Black;
  	Canvas->selectFont(&fabgl::FONT_8x8);
  	Canvas->setGlyphOptions(GlyphOptions().FillBackground(true));
  	Canvas->setPenWidth(1);
  	Canvas->setPenColor(fg);
  	Canvas->setBrushColor(bg);	
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
			send_packet(0x01, sizeof packet, packet);
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
//			digitalWrite(GPIO_ITRP, LOW);
//			digitalWrite(GPIO_ITRP, HIGH);
//			digitalWrite(GPIO_ITRP, LOW);
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
    	case 32 ... 126:
      		Canvas->drawTextFmt(charX, charY, "%c", c);
      		cursorRight();
      	break;
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

// Handle GCOL
// 
void vdu_gcol() {
  	byte mode = readByte();
  	fg.R = readByte();
  	fg.G = readByte();
  	fg.B = readByte();
	debug_log("vdu_gcol: %d,%d,%d,%d\n\r", mode, fg.R, fg.G, fg.B);
 	Canvas->setPenColor(fg);
}

// Handle PLOT
//
void vdu_plot() {
  	byte mode = readByte();
  	p3 = p2;
  	p2 = p1;
  	p1.X = readWord();
  	p1.Y = readWord();
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
  	Canvas->setBrushColor(fg);
  	Canvas->fillPath(p, 3);
  	Canvas->setBrushColor(bg);
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
//
void vdu_sys() {
  	byte mode = readByte();
	debug_log("vdu_sys: %d\n\r", mode);
  	switch(mode) {
	    case 0:
      		vdu_sys_esp();
      		break;
  	}
}

void vdu_sys_esp() {
  	byte mode = readByte();
  	switch(mode) {
  	}
}

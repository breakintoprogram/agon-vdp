#include <Arduino.h>

#include "agon.h"
#include "cursor.h"
#include "vdu_audio.h"
#include "vdu_sys.h"

extern void cls(bool resetViewports);
extern void clg();
extern void set_mode(int mode);
extern bool doWaitCompletion;

// TODO move to graphics handler file:

// Get the paint options for a given GCOL mode
//
fabgl::PaintOptions getPaintOptions(int mode, fabgl::PaintOptions priorPaintOptions) {
	fabgl::PaintOptions p = priorPaintOptions;
	
	switch(mode) {
		case 0: p.NOT = 0; p.swapFGBG = 0; break;
		case 4: p.NOT = 1; p.swapFGBG = 0; break;
	}
	return p;
}

// Set a palette item
// Parameters:
// - l: The logical colour to change
// - c: The new colour
// 
void setPaletteItem(int l, RGB888 c) {
	if(l < VGAColourDepth) {
		switch(VGAColourDepth) {
			case 2: VGAController2.setPaletteItem(l, c); break;
			case 4: VGAController4.setPaletteItem(l, c); break;
			case 8: VGAController8.setPaletteItem(l, c); break;
			case 16: VGAController16.setPaletteItem(l, c); break;
		}
	}
}



// VDU 17 Handle COLOUR
// 
void vdu_colour() {
	int		colour = readByte_t();
	byte	c = palette[colour%VGAColourDepth];

	if(colour >= 0 && colour < 64) {
		tfg = colourLookup[c];
		debug_log("vdu_colour: tfg %d = %02X : %02X,%02X,%02X\n\r", colour, c, tfg.R, tfg.G, tfg.B);
	}
	else if(colour >= 128 && colour < 192) {
		tbg = colourLookup[c];
		debug_log("vdu_colour: tbg %d = %02X : %02X,%02X,%02X\n\r", colour, c, tbg.R, tbg.G, tbg.B);	
	}
	else {
		debug_log("vdu_colour: invalid colour %d\n\r", colour);
	}
}

// VDU 18 Handle GCOL
// 
void vdu_gcol() {
	int		mode = readByte_t();
	int		colour = readByte_t();
	
	byte	c = palette[colour%VGAColourDepth];

	if(mode >= 0 && mode <= 6) {
		if(colour >= 0 && colour < 64) {
			gfg = colourLookup[c];
			debug_log("vdu_gcol: mode %d, gfg %d = %02X : %02X,%02X,%02X\n\r", mode, colour, c, gfg.R, gfg.G, gfg.B);
		}
		else if(colour >= 128 && colour < 192) {
			gbg = colourLookup[c];
			debug_log("vdu_gcol: mode %d, gbg %d = %02X : %02X,%02X,%02X\n\r", mode, colour, c, gbg.R, gbg.G, gbg.B);
		}
		else {
			debug_log("vdu_gcol: invalid colour %d\n\r", colour);
		}
		gpo = getPaintOptions(mode, gpo);
	}
	else {
		debug_log("vdu_gcol: invalid mode %d\n\r", mode);
	}
}

// VDU 19 Handle palette
//
void vdu_palette() {
	int l = readByte_t(); if(l == -1) return; // Logical colour
	int p = readByte_t(); if(p == -1) return; // Physical colour
	int r = readByte_t(); if(r == -1) return; // The red component
	int g = readByte_t(); if(g == -1) return; // The green component
	int b = readByte_t(); if(b == -1) return; // The blue component

	RGB888 col;				// The colour to set

	if(VGAColourDepth < 64) {			// If it is a paletted video mode
		if(p == 255) {					// If p = 255, then use the RGB values
			col = RGB888(r, g, b);
		}
		else if(p < 64) {				// If p < 64, then look the value up in the colour lookup table
			col = colourLookup[p];
		}
		else {				
			debug_log("vdu_palette: p=%d not supported\n\r", p);
			return;
		}
		setPaletteItem(l, col);
		doWaitCompletion = true;
		debug_log("vdu_palette: %d,%d,%d,%d,%d\n\r", l, p, r, g, b);
	}
	else {
		debug_log("vdu_palette: not supported in this mode\n\r");
	}
}

// VDU 22 Handle MODE
//
void vdu_mode() {
  	int mode = readByte_t();
	debug_log("vdu_mode: %d\n\r", mode);
	if(mode >= 0) {
	  	set_mode(mode);
	}
}

// VDU 24 Graphics viewport
// Example: VDU 24,640;256;1152;896;
//
void vdu_graphicsViewport() {
	int x1 = readWord_t();			// Left
	int y2 = readWord_t();			// Bottom
	int x2 = readWord_t();			// Right
	int y1 = readWord_t();			// Top

	if (setGraphicsViewport(x1, y1, x2, y2)) {
		debug_log("vdu_graphicsViewport: OK %d,%d,%d,%d\n\r", p1.X, p1.Y, p2.X, p2.Y);
	} else {
		debug_log("vdu_graphicsViewport: Invalid Viewport %d,%d,%d,%d\n\r", p1.X, p1.Y, p2.X, p2.Y);
	}
}

void vdu_plot_triangle(byte mode) {
  	Point p[3] = {
		p3,
		p2,
		p1, 
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
      		Canvas->drawEllipse(p2.X, p2.Y, r, r);
      		break;
    	case 0x94 ... 0x97: // Circle
      		a = p2.X - p1.X;
      		b = p2.Y - p1.Y;
      		r = 2 * sqrt(a * a + b * b);
      		Canvas->drawEllipse(p2.X, p2.Y, r, r);
      		break;
  	}
}

// VDU 25 Handle PLOT
//
void vdu_plot() {
  	int mode = readByte_t(); if(mode == -1) return;

	int x = readWord_t(); if(x == -1) return; else x = (short)x;
	int y = readWord_t(); if(y == -1) return; else y = (short)y;

  	p3 = p2;
  	p2 = p1;
	p1 = translateViewport(scale(x, y));
	Canvas->setClippingRect(graphicsViewport);
	Canvas->setPenColor(gfg);
	Canvas->setPaintOptions(gpo);
	debug_log("vdu_plot: mode %d, (%d,%d) -> (%d,%d)\n\r", mode, x, y, p1.X, p1.Y);
  	switch(mode) {
    	case 0x04: 			// Move 
      		Canvas->moveTo(p1.X, p1.Y);
      		break;
    	case 0x05: 			// Line
      		Canvas->lineTo(p1.X, p1.Y);
      		break;
		case 0x40 ... 0x47:	// Point
			Canvas->setPixel(p1.X, p1.Y);
			break;
    	case 0x50 ... 0x57: // Triangle
      		vdu_plot_triangle(mode);
      		break;
    	case 0x90 ... 0x97: // Circle
      		vdu_plot_circle(mode);
      		break;
  	}
	doWaitCompletion = true;
}

// VDU 26 Reset graphics and text viewports
//
void vdu_resetViewports() {
	viewportReset();
    // TODO reset cursors too (according to BBC BASIC manual)
	debug_log("vdu_resetViewport\n\r");
}

// VDU 28: text viewport
// Example: VDU 28,20,23,34,4
//
void vdu_textViewport() {
	int x1 = readByte_t() * fontW;				// Left
	int y2 = (readByte_t() + 1) * fontH - 1;	// Bottom
	int x2 = (readByte_t() + 1) * fontW - 1;	// Right
	int y1 = readByte_t() * fontH;				// Top

	if (setTextViewport(x1, y1, x2, y2)) {
		ensureCursorInViewport(textViewport);
		debug_log("vdu_textViewport: OK %d,%d,%d,%d\n\r", x1, y1, x2, y2);
	} else {
		debug_log("vdu_textViewport: Invalid Viewport %d,%d,%d,%d\n\r", x1, y1, x2, y2);
	}
}

// Handle VDU 29
//
void vdu_origin() {
	int x = readWord_t();
	if(x >= 0) {
		int y = readWord_t();
		if(y >= 0) {
			setOrigin(x, y);
			debug_log("vdu_origin: %d,%d\n\r", x, y);
		}
	}
}

// VDU 30 TAB(x,y)
//
void vdu_cursorTab() {
	int x = readByte_t();
	if (x >= 0) {
		int y = readByte_t();
		if (y >= 0) {
            cursorTab(x, y);
		}
	}
}

// Handle VDU commands
//
void vdu(byte c) {
	bool useTextCursor = (activeCursor == &textCursor);

    doWaitCompletion = false;
    switch(c) {
        case 0x04:	
            // enable text cursor
            Canvas->setGlyphOptions(GlyphOptions().FillBackground(true));
            activeCursor = &textCursor;
            activeViewport = &textViewport;
            break;
        case 0x05:
            // enable graphics cursor
            Canvas->setGlyphOptions(GlyphOptions().FillBackground(false));
            activeCursor = &p1;
            activeViewport = &graphicsViewport;
            break;
        case 0x07:	// Bell
            play_note(0, 100, 750, 125);
            break;
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
            cls(false);
            break;
        case 0x0D:  // CR
            cursorCR();
            break;
        case 0x0E:	// Paged mode ON
            setPagedMode(true);
            break;
        case 0x0F:	// Paged mode OFF
            setPagedMode(false);
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
        case 0x13:	// Define Logical Colour
            vdu_palette();
            break;
        case 0x16:  // Mode
            vdu_mode();
            break;
        case 0x17:  // VDU 23
            vdu_sys();
            break;
        case 0x18:	// Define a graphics viewport
            vdu_graphicsViewport();
            break;
        case 0x19:  // PLOT
            vdu_plot();
            break;
        case 0x1A:	// Reset text and graphics viewports
            vdu_resetViewports();
            break;
        case 0x1C:	// Define a text viewport
            vdu_textViewport();
            break;
        case 0x1D:	// VDU_29
            vdu_origin();
        case 0x1E:	// Move cursor to top left of the viewport
            cursorHome();
            break;
        case 0x1F:	// TAB(X,Y)
            vdu_cursorTab();
            break;
        case 0x20 ... 0x7E:
        case 0x80 ... 0xFF:
            if (useTextCursor) {
                Canvas->setClippingRect(defaultViewport);
                Canvas->setPenColor(tfg);
                Canvas->setBrushColor(tbg);
                Canvas->setPaintOptions(tpo);
            }
            else {
                Canvas->setClippingRect(graphicsViewport);
                Canvas->setPenColor(gfg);
                Canvas->setPaintOptions(gpo);
            }
            Canvas->drawChar(activeCursor->X, activeCursor->Y, c);
            cursorRight();
            break;
        case 0x7F:  // Backspace
            cursorLeft();
            Canvas->setBrushColor(useTextCursor ? tbg : gbg);
            Canvas->fillRectangle(activeCursor->X, activeCursor->Y, activeCursor->X + fontW - 1, activeCursor->Y + fontH - 1);
            break;
    }
    if (doWaitCompletion) {
        Canvas->waitCompletion(false);
    }
}

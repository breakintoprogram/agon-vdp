#ifndef CURSOR_H
#define CURSOR_H

#include <fabgl.h>
#include <Arduino.h>

#include "agon_keyboard.h"
#include "vdp_protocol.h"
#include "viewport.h"

extern int		fontW;
extern int		fontH;
extern fabgl::Canvas * Canvas;
extern void wait_shiftkey();	// defined by agon_keyboard.h

Point			textCursor;						// Text cursor
Point *			activeCursor;					// Pointer to the active text cursor (textCursor or p1)
bool			cursorEnabled = true;			// Cursor visibility
byte			cursorBehaviour = 0;			// Cursor behavior
bool 			pagedMode = false;				// Is output paged or not? Set by VDU 14 and 15
int				pagedModeCount = 0;				// Scroll counter for paged mode


// Render a cursor at the current screen position
//
void do_cursor() {
	if (cursorEnabled) {
		Canvas->swapRectangle(textCursor.X, textCursor.Y, textCursor.X + fontW - 1, textCursor.Y + fontH - 1);
	}
}

// Move the active cursor to the leftmost position in the viewport
//
void cursorCR() {
  	activeCursor->X = activeViewport->X1;
}

// Move the active cursor down a line
//
void cursorDown() {
	activeCursor->Y += fontH;
	//
	// If in graphics mode, then don't scroll, wrap to top
	// 
	if (activeCursor != &textCursor) {
		if (activeCursor->Y > activeViewport->Y2) {	
			activeCursor->Y = activeViewport->Y2;
		}
		return;
	}
	//
	// Using the text cursor, so handle paging and scrolling
	//
	if (pagedMode) {
		pagedModeCount++;
		if (pagedModeCount >= (activeViewport->Y2 - activeViewport->Y1 + 1) / fontH) {
			pagedModeCount = 0;
			wait_shiftkey();
		}
	}
	//
	// Check if scroll required
	//
	if (activeCursor->Y > activeViewport->Y2) {
		activeCursor->Y -= fontH;
		if (~cursorBehaviour & 0x01) {
			Canvas->setScrollingRegion(activeViewport->X1, activeViewport->Y1, activeViewport->X2, activeViewport->Y2);
			Canvas->scroll(0, -fontH);
		}
		else {
			activeCursor->X = activeViewport->X2 + 1;
		}
	}
}

// Move the active cursor up a line
//
void cursorUp() {	
  	activeCursor->Y -= fontH;
  	if (activeCursor->Y < activeViewport->Y1) {
		activeCursor->Y = activeViewport->Y1;
  	}
}

// Move the active cursor back one character
//
void cursorLeft() {
	activeCursor->X -= fontW;											
	if (activeCursor->X < activeViewport->X1) {								// If moved past left edge of active viewport then
		activeCursor->X = activeViewport->X1;								// Lock it there
	}
}

// Advance the active cursor right one character
//
void cursorRight() {
  	activeCursor->X += fontW;											
  	if (activeCursor->X > activeViewport->X2) {								// If advanced past right edge of active viewport
		if (activeCursor == &textCursor || (~cursorBehaviour & 0x40)) {		// If it is a text cursor or VDU 5 CR/LF is enabled then
			cursorCR();														// Do carriage return
   			cursorDown();													// and line feed
		}
  	}
}

// Move the active cursor to the top-left position in the viewport
//
void cursorHome() {
	activeCursor->X = activeViewport->X1;
	activeCursor->Y = activeViewport->Y1;
}

// TAB(x,y)
//
void cursorTab(int x, int y) {
	activeCursor->X = x * fontW;
	activeCursor->Y = y * fontH;
}

// Reset basic cursor control
//
void resetCursor() {
	activeCursor = &textCursor;
	cursorEnabled = true;
	cursorBehaviour = 0;
	pagedMode = false;
}

void homeCursor() {
	textCursor = Point(activeViewport->X1, activeViewport->Y1);
}

void enableCursor(bool enable = true) {
	cursorEnabled = enable;
}

void setPagedMode(bool mode = pagedMode) {
	pagedMode = mode;
	pagedModeCount = 0;
}

void ensureCursorInViewport(Rect viewport) {
	if (activeCursor->X < viewport.X1
		|| activeCursor->X > viewport.X2
		|| activeCursor->Y < viewport.Y1
		|| activeCursor->Y > viewport.Y2
		) {
		activeCursor->X = viewport.X1;
		activeCursor->Y = viewport.Y1;
	}
}

void setCursorBehaviour(byte setting, byte mask = 0xFF) {
	cursorBehaviour = (cursorBehaviour & mask) ^ setting;
}

#endif	// CURSOR_H

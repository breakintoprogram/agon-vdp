#ifndef VIEWPORT_H
#define VIEWPORT_H

#include <fabgl.h>

#include "agon.h"
#include "cursor.h"

extern fabgl::Canvas * Canvas;

Point			origin;							// Screen origin
int				canvasW;						// Canvas width
int				canvasH;						// Canvas height
bool			logicalCoords = true;			// Use BBC BASIC logical coordinates
double			logicalScaleX;					// Scaling factor for logical coordinates
double			logicalScaleY;
Rect *			activeViewport;					// Pointer to the active text viewport (textViewport or graphicsViewport)
Rect 			defaultViewport;				// Default viewport
Rect 			textViewport;					// Text viewport
Rect 			graphicsViewport;				// Graphics viewport
bool			useViewports = false;			// Viewports are enabled

// Reset viewports to default
//
void viewportReset() {
	defaultViewport = Rect(0, 0, canvasW - 1, canvasH - 1);
	textViewport =	Rect(0, 0, canvasW - 1, canvasH - 1);
	graphicsViewport = Rect(0, 0, canvasW - 1, canvasH - 1);
	activeViewport = &textViewport;
	useViewports = false;
}

// Clear a viewport
//
void clearViewport(Rect * viewport) {
	if (Canvas) {
		if (useViewports) {
			Canvas->fillRectangle(*viewport);
		}
		else {
			Canvas->clear();
		}
	}
}

// Translate a point relative to the graphics viewport
//
Point translateViewport(int X, int Y) {
	if (logicalCoords) {
		return Point(graphicsViewport.X1 + (origin.X + X), graphicsViewport.Y2 - (origin.Y + Y));
	}
	return Point(graphicsViewport.X1 + (origin.X + X), graphicsViewport.Y1 + (origin.Y + Y));
}
Point translateViewport(Point p) {
	return translateViewport(p.X, p.Y);
}

// Scale a point
//
Point scale(int X, int Y) {
	if (logicalCoords) {
		return Point((double)X / logicalScaleX, (double)Y / logicalScaleY);
	}
	return Point(X, Y);
}
Point scale(Point p) {
	return scale(p.X, p.Y);
}

// Translate a point relative to the canvas
//
Point translateCanvas(int X, int Y) {
	if (logicalCoords) {
		return Point(origin.X + X, (canvasH - 1) - (origin.Y + Y));
	}
	return Point(origin.X + X, origin.Y + Y);
}
Point translateCanvas(Point p) {
	return translateCanvas(p.X, p.Y);
}

// Set graphics viewport
bool setGraphicsViewport(short x1, short y1, short x2, short y2) {
	Point p1 = translateCanvas(scale((short)x1, (short)y1));
	Point p2 = translateCanvas(scale((short)x2, (short)y2));

	if (p1.X >= 0 && p2.X < canvasW && p1.Y >= 0 && p2.Y < canvasH && p2.X > p1.X && p2.Y > p1.Y) {
		graphicsViewport = Rect(p1.X, p1.Y, p2.X, p2.Y);
		useViewports = true;
    	Canvas->setClippingRect(graphicsViewport);
        return true;
    }
    return false;
}

// Set text viewport
bool setTextViewport(short x1, short y1, short x2, short y2) {
	if (x2 >= canvasW) x2 = canvasW - 1;
	if (y2 >= canvasH) y2 = canvasH - 1;

	if (x1 >= 0 && y1 >= 0 && x2 > x1 && y2 > y1) {
		textViewport = Rect(x1, y1, x2, y2);
		useViewports = true;
		return true;
	}
    return false;
}

inline void setOrigin(int x, int y) {
    origin = scale(x, y);
}

#endif // VIEWPORT_H

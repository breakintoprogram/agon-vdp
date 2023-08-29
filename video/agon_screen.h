#ifndef AGON_SCREEN_H
#define AGON_SCREEN_H

#include <fabgl.h>

fabgl::Canvas *				Canvas;				// The canvas class

fabgl::VGA2Controller		VGAController2;		// VGA class - 2 colours
fabgl::VGA4Controller		VGAController4;		// VGA class - 4 colours
fabgl::VGA8Controller		VGAController8;		// VGA class - 8 colours
fabgl::VGA16Controller		VGAController16;	// VGA class - 16 colours
fabgl::VGAController		VGAController64;	// VGA class - 64 colours

fabgl::VGABaseController *	VGAController;		// Pointer to the current VGA controller class (one of the above)

int				_VGAColourDepth;				// Number of colours per pixel (2, 4, 8, 16 or 64)
bool			doubleBuffered = false;			// Disable double buffering by default


// Get controller
// Parameters:
// - colours: Number of colours per pixel (2, 4, 8, 16 or 64)
// Returns:
// - A singleton instance of a VGAController class
//
fabgl::VGABaseController * getVGAController(int colours = _VGAColourDepth) {
	switch (colours) {
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
	switch (_VGAColourDepth) {
		case 2: VGAController2.updateRGB2PaletteLUT(); break;
		case 4: VGAController4.updateRGB2PaletteLUT(); break;
		case 8: VGAController8.updateRGB2PaletteLUT(); break;
		case 16: VGAController16.updateRGB2PaletteLUT(); break;
	}
}

// Get current colour depth
//
inline int getVGAColourDepth() {
	return _VGAColourDepth;
}

// Set a palette item
// Parameters:
// - l: The logical colour to change
// - c: The new colour
// 
void setPaletteItem(int l, RGB888 c) {
	int depth = getVGAColourDepth();
	if (l < depth) {
		switch (depth) {
			case 2: VGAController2.setPaletteItem(l, c); break;
			case 4: VGAController4.setPaletteItem(l, c); break;
			case 8: VGAController8.setPaletteItem(l, c); break;
			case 16: VGAController16.setPaletteItem(l, c); break;
		}
	}
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
	fabgl::VGABaseController * controller = getVGAController(colours);

	if (controller == nullptr) {					// If controller is null, then an invalid # of colours was passed
		return 1;									// So return the error
	}
	delete Canvas;									// Delete the canvas

	_VGAColourDepth = colours;						// Set the number of colours per pixel
	if (VGAController != controller) {				// Is it a different controller?
		if (VGAController) {						// If there is an existing controller running then
			VGAController->end();					// end it
		}
		VGAController = controller;					// Switch to the new controller
		VGAController->begin();						// And spin it up
	}
	if (modeLine) {									// If modeLine is not a null pointer then
		if (doubleBuffered == true) {
			VGAController->setResolution(modeLine, -1, -1, 1);
		} else {
			VGAController->setResolution(modeLine);	// Set the resolution
		}
	} else {
		debug_log("change_resolution: modeLine is null\n\r");
	}
	VGAController->enableBackgroundPrimitiveExecution(true);
	VGAController->enableBackgroundPrimitiveTimeout(false);

	Canvas = new fabgl::Canvas(VGAController);		// Create the new canvas
	//
	// Check whether the selected mode has enough memory for the vertical resolution
	//
	if (VGAController->getScreenHeight() != VGAController->getViewPortHeight()) {
		return 2;
	}
	return 0;										// Return with no errors
}

// Swap to other buffer if we're in a double-buffered mode
//
void switchBuffer() {
	if (doubleBuffered == true) {
		Canvas->swapBuffers();
	}
}

// Wait for plot completion
//
void waitPlotCompletion() {
	Canvas->waitCompletion(false);
}

#endif // AGON_SCREEN_H

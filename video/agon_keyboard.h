#ifndef AGON_KEYBOARD_H
#define AGON_KEYBOARD_H

#include <Arduino.h>
#include <fabgl.h>

#include "agon.h"
#include "cursor.h"
#include "vdp_protocol.h"

fabgl::PS2Controller		PS2Controller;		// The keyboard class

byte 			keycode = 0;					// Last pressed key code
byte 			modifiers = 0;					// Last pressed key modifiers
int				kbRepeatDelay = 500;			// Keyboard repeat delay ms (250, 500, 750 or 1000)		
int				kbRepeatRate = 100;				// Keyboard repeat rate ms (between 33 and 500)

// TODO refactor out sending keyboard info via VDP protocol

// Keyboard setup
//
void setupKeyboard() {
 	PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
	PS2Controller.keyboard()->setLayout(&fabgl::UKLayout);
	PS2Controller.keyboard()->setCodePage(fabgl::CodePages::get(1252));
	PS2Controller.keyboard()->setTypematicRateAndDelay(kbRepeatRate, kbRepeatDelay);
}

// Set keyboard layout
//
void setKeyboardLayout(int region) {
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

// Handle the keyboard: BBC VDU Mode
//
void do_keyboard() {
  	fabgl::Keyboard* kb = PS2Controller.keyboard();
	fabgl::VirtualKeyItem item;

	#if SERIALKB == 1
	if (DBGSerial.available()) {
		byte packet[] = {
			DBGSerial.read(),
			0,
		};
		send_packet(PACKET_KEYCODE, sizeof packet, packet);
		return;
	}
	#endif
	
	if (kb->getNextVirtualKey(&item, 0)) {
		if (item.down) {
			switch (item.vk) {
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
		switch (keycode) {
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

// Handle the keyboard: CP/M Terminal Mode
// 
void do_keyboard_terminal() {
  	fabgl::Keyboard* kb = PS2Controller.keyboard();
	fabgl::VirtualKeyItem item;

	// Read the keyboard and transmit to the Z80
	//
	if (kb->getNextVirtualKey(&item, 0)) {
		if (item.down) {
			writeByte(item.ASCII);
		}
	}

	// Wait for the response and write to the screen
	//
	while (byteAvailable()) {
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
	} while (item.SHIFT);

	// And pressed again
	//
	do {
		kb->getNextVirtualKey(&item, 0);
		if (item.ASCII == 27) {	// Check for ESC
			byte packet[] = {
				item.ASCII,
				0,
				item.vk,
				item.down,
			};
			send_packet(PACKET_KEYCODE, sizeof packet, packet);
			return;
		}
	} while (!item.SHIFT);
}

void getKeyboardState(int *repeatDelay, int *repeatRate, byte *ledState) {
    bool numLock;
	bool capsLock;
	bool scrollLock;
	PS2Controller.keyboard()->getLEDs(&numLock, &capsLock, &scrollLock);
	*ledState = scrollLock | (capsLock << 1) | (numLock << 2);
    *repeatDelay = kbRepeatDelay;
    *repeatRate = kbRepeatRate;
}

void setKeyboardState(int delay, int rate, byte ledState) {
    if (delay >= 250 && delay <= 1000) kbRepeatDelay = (delay / 250) * 250;	// In steps of 250ms
	if (rate >=  33 && rate <=  500) kbRepeatRate  = rate;
	if (ledState != 255) {
		PS2Controller.keyboard()->setLEDs(ledState & 4, ledState & 2, ledState & 1);
	}
    PS2Controller.keyboard()->setTypematicRateAndDelay(kbRepeatRate, kbRepeatDelay);
}

#endif // AGON_KEYBOARD_H

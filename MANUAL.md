# manual

The VDP Graphics Terminal is based upon the BBC Micro VDU command.

Whilst it is tailored to work well with the Agon version of BBC Basic for Z80, the command set should be sufficiently rich for any application to render graphics on-screen.

## Interface

The interface between the eZ80 and the VDP is via a serial UART running at 250,000 baud.

## VDU Character Sequences

The VDU command is a work-in-progress with a handful of mappings implemented.

- `VDU 8` Cursor left
- `VDU 9` Cursor right
- `VDU 10` Cursor down
- `VDU 11` Cursor up
- `VDU 12` CLS
- `VDU 13` Carriage return
- `VDU 18,mode,r,g,b` GCOL mode,r,g,b
- `VDU 22,n` Mode n
- `VDU 25,mode,x;y;` PLOT mode,x,y
- `VDU 30` Home cursor
- `VDU 31,x,y` TAB(x,y)
- `VDU 127` Backspace

All other characters are sent to the screen as ASCII, unaltered.

## Serial Protocol

Data sent from the VPD to the eZ80's UART0 is sent as a packet in the following format:

- cmd: The packet command, with bit 7 set
- len: Number of data bytes
- data: The data byte(s)

## Keyboard

When a key is pressed, a packet is sent with the following data:
- cmd: 0x01
- keycode: The ASCII value of the key pressed
- modifiers: A byte with the following bits set (1 = pressed):
	0. CTRL
	1. SHIFT
	2. ALT LEFT
	3. ALT RIGHT
	4. CAPS LOCK
	5. NUM LOCK
	6. SCROLL LOCK
	7. GUI
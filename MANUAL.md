# manual

The VDP Graphics Terminal is based upon the BBC Micro VDU command.

Whilst it is tailored to work well with the Agon version of BBC Basic for Z80, the command set should be sufficiently rich for any application to render graphics on-screen.

## Interface

The interface between the eZ80 and the VDP is via a serial UART running at 384,000 baud.

## VDU Character Sequences

The VDU command is a work-in-progress with a handful of mappings implemented.

- `VDU 8`: Cursor left
- `VDU 9`: Cursor right
- `VDU 10`: Cursor down
- `VDU 11`: Cursor up
- `VDU 12`: CLS
- `VDU 13`: Carriage return
- `VDU 18, mode, r, g, b`: GCOL mode, r, g, b
- `VDU 22, n`: Mode n
- `VDU 23, n`: UDG / System Commands
- `VDU 25, mode, x; y;`: PLOT mode, x, y
- `VDU 30`: Home cursor
- `VDU 31, x, y`: TAB(x, y)
- `VDU 127`: Backspace

All other characters are sent to the screen as ASCII, unaltered.

## VDU 23, 0: VPD commands

VDU 23, 0 is reserved for commands sent to the VDP

- `VDU 23, 0, 1, n`: Set the keyboard locale (0=UK, 1=US) 
- `VDU 23, 0, 2`: Request cursor position
- `VDU 23, 0, 3, x; y;`: Get ASCII code of character at character position x, y
- `VDU 23, 0, 4, x; y;`: Get colour of pixel at pixel position x, y
- `VDU 23, 0, 5, channel, waveform, volume, freq; duration;`: Send a note to the VDP audio driver
- `VDU 23, 0, 6`: Fetch the screen dimensions 

Commands 2 onwards will return their data back to the eZ80 via the serial protocol

## VDU 23, 27: Sprites and Bitmaps

VDU 23, 27 is reserved for the bitmap and sprite functionality

### Bitmaps

- `VDU 23, 27, 0, n`: Select bitmap n
- `VDU 23. 27, 1, n, w; h; b1, b2 ... bn`: Load colour bitmap data into current bitmap
- `VDU 23, 27, 2, n, w; h; col1; col2; b1, b2 ... bn`: Load monochrome bitmap data into current bitmap
- `VDU 23, 27, 3, x; y;`: Draw current bitmap on screen at pixel position x, y

### Sprites

- `VDU 23, 27, 4, n`: Select sprite n
- `VDU 23, 27, 5`: Clear frames in current sprite
- `VDU 23, 27, 6, n`: Add bitmap n as a frame to current sprite
- `VDU 23, 27, 7, n`: Activate n sprites
- `VDU 23, 27, 8`: Select next frame of current sprite
- `VDU 23, 27, 9`: Select previous frame of current sprite
- `VDU 23, 27, 10, n`: Select the nth frame of current sprite
- `VDU 23, 27, 11`: Show current sprite
- `VDU 23, 27, 12`: Hide current sprite
- `VDU 23, 27, 13, x; y;`: Move current sprite to pixel position x, y
- `VDU 23, 27, 14, x; y;`: Move current sprite by x, y pixels
- `VDU 23, 27, 15`: Update the sprites in the GPU

## Serial Protocol

Data sent from the VPD to the eZ80's UART0 is sent as a packet in the following format:

- cmd: The packet command, with bit 7 set
- len: Number of data bytes
- data: The data byte(s)

Words are 16 bit, and sent in little-endian format

Packets:

- `0x00`: General Poll
- `0x01, keycode, modifiers`: Keyboard
- `0x02, x, y`: Cursor position
- `0x03, char`: Character read from screen
- `0x04, r, g, b`: Pixel colour read from screen
- `0x05, channel, success`: Audio play note acknowledgement
- `0x06, width, height, cols, rows`: Screen dimensions - width and height are words

## Keyboard

When a key is pressed, a packet is sent with the following data:
- cmd: 0x01
- keycode: The ASCII value of the key pressed
- modifiers: A byte with the following bits set (1 = pressed):
```
0. CTRL
1. SHIFT
2. ALT LEFT
3. ALT RIGHT
4. CAPS LOCK
5. NUM LOCK
6. SCROLL LOCK
7. GUI
```
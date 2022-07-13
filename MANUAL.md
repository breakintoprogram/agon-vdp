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

## Keyboard

Reading the terminal currently returns the ASCII code of the key that has been pressed.
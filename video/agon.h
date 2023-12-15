//
// Title:			Agon Video BIOS - Function prototypes
// Author:			Dean Belfield
// Created:			05/09/2022
// Last Updated:	13/08/2023
//
// Modinfo:
// 04/03/2023:		Added LOGICAL_SCRW and LOGICAL_SCRH
// 17/03/2023:		Added PACKET_RTC, EPOCH_YEAR, MAX_SPRITES, MAX_BITMAPS
// 21/03/2023:		Added PACKET_KEYSTATE
// 22/03/2023:		Added VDP codes
// 23/03/2023:		Increased baud rate to 1152000
// 09/08/2023:		Added VDP_SWITCHBUFFER
// 13/08/2023:		Added additional modelines

#pragma once

#define EPOCH_YEAR				1980	// 1-byte dates are offset from this (for FatFS)
#define MAX_SPRITES				256		// Maximum number of sprites
#define MAX_BITMAPS				256		// Maximum number of bitmaps

#define UART_BR					1152000	// Max baud rate; previous stable value was 384000
#define UART_NA					-1
#define UART_TX					2
#define UART_RX					34
#define UART_RTS				13		// The ESP32 RTS pin (eZ80 CTS)
#define UART_CTS	 			14		// The ESP32 CTS pin (eZ80 RTS)

#define COMMS_TIMEOUT			200		// Timeout for VDP commands (ms)

#define UART_RX_SIZE			256		// The RX buffer size
#define UART_RX_THRESH			128		// Point at which RTS is toggled

#define GPIO_ITRP				17		// VSync Interrupt Pin - for reference only

// Commands for VDU 23, 0, n
//
#define VDP_GP					0x80	// General poll data
#define VDP_KEYCODE				0x81	// Keyboard data
#define VDP_CURSOR				0x82	// Cursor positions
#define VDP_SCRCHAR				0x83	// Character read from screen
#define VDP_SCRPIXEL			0x84	// Pixel read from screen
#define VDP_AUDIO				0x85	// Audio commands
#define VDP_MODE				0x86	// Get screen dimensions
#define VDP_RTC					0x87	// RTC
#define VDP_KEYSTATE			0x88	// Keyboard repeat rate and LED status
#define VDP_MOUSE				0x89	// Mouse data
#define VDP_BUFFERED			0xA0	// Buffered commands
#define VDP_UPDATER				0xA1	// Update VDP
#define VDP_LOGICALCOORDS		0xC0	// Switch BBC Micro style logical coords on and off
#define VDP_LEGACYMODES			0xC1	// Switch VDP 1.03 compatible modes on and off
#define VDP_SWITCHBUFFER		0xC3	// Double buffering control
#define VDP_CONSOLEMODE			0xFE	// Switch console mode on and off
#define VDP_TERMINALMODE		0xFF	// Switch to terminal mode

// And the corresponding return packets
// By convention, these match their VDP counterpart, but with the top bit reset
//
#define PACKET_GP				0x00	// General poll data
#define PACKET_KEYCODE			0x01	// Keyboard data
#define PACKET_CURSOR			0x02	// Cursor positions
#define PACKET_SCRCHAR			0x03	// Character read from screen
#define PACKET_SCRPIXEL			0x04	// Pixel read from screen
#define PACKET_AUDIO			0x05	// Audio acknowledgement
#define PACKET_MODE				0x06	// Get screen dimensions
#define PACKET_RTC				0x07	// RTC
#define PACKET_KEYSTATE			0x08	// Keyboard repeat rate and LED status
#define PACKET_MOUSE			0x09	// Mouse data

#define AUDIO_CHANNELS			3		// Default number of audio channels
#define AUDIO_DEFAULT_SAMPLE_RATE	FABGL_SOUNDGEN_DEFAULT_SAMPLE_RATE	// Default sample rate
#define MAX_AUDIO_CHANNELS		32		// Maximum number of audio channels
#define PLAY_SOUND_PRIORITY		3		// Sound driver task priority with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest

// Audio command definitions
//
#define AUDIO_CMD_PLAY			0		// Play a sound
#define AUDIO_CMD_STATUS		1		// Get the status of a channel
#define AUDIO_CMD_VOLUME		2		// Set the volume of a channel
#define AUDIO_CMD_FREQUENCY		3		// Set the frequency of a channel
#define AUDIO_CMD_WAVEFORM		4		// Set the waveform type for a channel
#define AUDIO_CMD_SAMPLE		5		// Sample management
#define AUDIO_CMD_ENV_VOLUME	6		// Define/set a volume envelope
#define AUDIO_CMD_ENV_FREQUENCY	7		// Define/set a frequency envelope
#define AUDIO_CMD_ENABLE		8		// Enables a channel
#define AUDIO_CMD_DISABLE		9		// Disables (destroys) a channel
#define AUDIO_CMD_RESET			10		// Reset audio channel

#define AUDIO_WAVE_DEFAULT		0		// Default waveform (Square wave)
#define AUDIO_WAVE_SQUARE		0		// Square wave
#define AUDIO_WAVE_TRIANGLE		1		// Triangle wave
#define AUDIO_WAVE_SAWTOOTH		2		// Sawtooth wave
#define AUDIO_WAVE_SINE			3		// Sine wave
#define AUDIO_WAVE_NOISE		4		// Noise (simple, no frequency support)
#define AUDIO_WAVE_VICNOISE		5		// VIC-style noise (supports frequency)
#define AUDIO_WAVE_SAMPLE		8		// Sample playback, explicit buffer ID sent in following 2 bytes
// negative values for waveforms indicate a sample number

#define AUDIO_SAMPLE_LOAD		0		// Send a sample to the VDP
#define AUDIO_SAMPLE_CLEAR		1		// Clear/delete a sample
#define AUDIO_SAMPLE_FROM_BUFFER	2	// Load a sample from a buffer
#define AUDIO_SAMPLE_DEBUG_INFO 0x10	// Get debug info about a sample

#define AUDIO_DEFAULT_FREQUENCY	523	// Default sample frequency (C5, or C above middle C)

#define AUDIO_FORMAT_8BIT_SIGNED	0	// 8-bit signed sample
#define AUDIO_FORMAT_8BIT_UNSIGNED	1	// 8-bit unsigned sample
#define AUDIO_FORMAT_DATA_MASK		7	// data bit mask for format
#define AUDIO_FORMAT_WITH_RATE		8	// OR this with the format to indicate a sample rate follows
#define AUDIO_FORMAT_TUNEABLE		16	// OR this with the format to indicate sample can be tuned (frequency adjustable)

#define AUDIO_ENVELOPE_NONE		0		// No envelope
#define AUDIO_ENVELOPE_ADSR		1		// Simple ADSR volume envelope

#define AUDIO_FREQUENCY_ENVELOPE_STEPPED	1		// Stepped frequency envelope

#define AUDIO_FREQUENCY_REPEATS 0x01	// Repeat/loop the frequency envelope
#define AUDIO_FREQUENCY_CUMULATIVE	0x02	// Reset frequency envelope when looping
#define AUDIO_FREQUENCY_RESTRICT	0x04	// Restrict frequency envelope to the range 0-65535

#define AUDIO_STATUS_ACTIVE		0x01	// Has an active waveform
#define AUDIO_STATUS_PLAYING	0x02	// Playing a note (not in release phase)
#define AUDIO_STATUS_INDEFINITE	0x04	// Indefinite duration sound playing
#define AUDIO_STATUS_HAS_VOLUME_ENVELOPE	0x08	// Channel has a volume envelope set
#define AUDIO_STATUS_HAS_FREQUENCY_ENVELOPE	0x10	// Channel has a frequency envelope set

enum AudioState : uint8_t {	// Audio channel state
	Idle = 0,				// currently idle/silent
	Pending,				// note will be played next loop call
	Playing,				// playing (passive)
	PlayLoop,				// active playing loop (used when an envelope is active)
	Release,				// in "release" phase
	Abort					// aborting a note
};

// Mouse commands
#define MOUSE_ENABLE			0		// Enable mouse
#define MOUSE_DISABLE			1		// Disable mouse
#define MOUSE_RESET				2		// Reset mouse
#define MOUSE_SET_CURSOR		3		// Set cursor
#define MOUSE_SET_POSITION		4		// Set mouse position
#define MOUSE_SET_AREA			5		// Set mouse area
#define MOUSE_SET_SAMPLERATE	6		// Set mouse sample rate
#define MOUSE_SET_RESOLUTION	7		// Set mouse resolution
#define MOUSE_SET_SCALING		8		// Set mouse scaling
#define MOUSE_SET_ACCERATION	9		// Set mouse acceleration (1-2000)
#define MOUSE_SET_WHEELACC		10		// Set mouse wheel acceleration

#define MOUSE_DEFAULT_CURSOR	0;		// Default mouse cursor
#define MOUSE_DEFAULT_SAMPLERATE	60;	// Default mouse sample rate
#define MOUSE_DEFAULT_RESOLUTION	2;	// Default mouse resolution (4 counts/mm)
#define MOUSE_DEFAULT_SCALING	1;		// Default mouse scaling (1:1)
#define MOUSE_DEFAULT_ACCELERATION	180;	// Default mouse acceleration 
#define MOUSE_DEFAULT_WHEELACC		60000;	// Default mouse wheel acceleration

// Buffered commands
#define BUFFERED_WRITE			0x00	// Write to a numbered buffer
#define BUFFERED_CALL			0x01	// Call buffered commands
#define BUFFERED_CLEAR			0x02	// Clear buffered commands
#define BUFFERED_CREATE			0x03	// Create a new empty buffer
#define BUFFERED_SET_OUTPUT		0x04	// Set the output buffer
#define BUFFERED_ADJUST			0x05	// Adjust buffered commands
#define BUFFERED_COND_CALL		0x06	// Conditionally call a buffer
#define BUFFERED_JUMP			0x07	// Jump to a buffer
#define BUFFERED_COND_JUMP		0x08	// Conditionally jump to a buffer
#define BUFFERED_OFFSET_JUMP	0x09	// Jump to a buffer with an offset
#define BUFFERED_OFFSET_COND_JUMP	0x0A	// Conditionally jump to a buffer with an offset
#define BUFFERED_OFFSET_CALL	0x0B	// Call a buffer with an offset
#define BUFFERED_OFFSET_COND_CALL	0x0C	// Conditionally call a buffer with an offset
#define BUFFERED_COPY			0x0D	// Copy blocks from multiple buffers into one buffer
#define BUFFERED_CONSOLIDATE	0x0E	// Consolidate blocks inside a buffer into one
#define BUFFERED_SPLIT			0x0F	// Split a buffer into multiple blocks
#define BUFFERED_SPLIT_INTO		0x10	// Split a buffer into multiple blocks to new buffer(s)
#define BUFFERED_SPLIT_FROM		0x11	// Split to new buffers from a target bufferId onwards
#define BUFFERED_SPLIT_BY		0x12	// Split a buffer into multiple blocks by width (columns)
#define BUFFERED_SPLIT_BY_INTO	0x13	// Split by width into new buffer(s)
#define BUFFERED_SPLIT_BY_FROM	0x14	// Split by width to new buffers from a target bufferId onwards
#define BUFFERED_SPREAD_INTO	0x15	// Spread blocks from a buffer to multiple target buffers
#define BUFFERED_SPREAD_FROM	0x16	// Spread blocks from target buffer ID onwards
#define BUFFERED_REVERSE_BLOCKS	0x17	// Reverse the order of blocks in a buffer
#define BUFFERED_REVERSE		0x18	// Reverse the order of data in a buffer

#define BUFFERED_DEBUG_INFO		0x20	// Get debug info about a buffer

// Adjust operation codes
#define ADJUST_NOT				0x00	// Adjust: NOT
#define ADJUST_NEG				0x01	// Adjust: Negative
#define ADJUST_SET				0x02	// Adjust: set new value (replace)
#define ADJUST_ADD				0x03	// Adjust: add
#define ADJUST_ADD_CARRY		0x04	// Adjust: add with carry
#define ADJUST_AND				0x05	// Adjust: AND
#define ADJUST_OR				0x06	// Adjust: OR
#define ADJUST_XOR				0x07	// Adjust: XOR

// Adjust operation flags
#define ADJUST_OP_MASK			0x0F	// operation code mask
#define ADJUST_ADVANCED_OFFSETS	0x10	// advanced, 24-bit offsets (16-bit block offset follows if top bit set)
#define ADJUST_BUFFER_VALUE		0x20	// operand is a buffer fetched value
#define ADJUST_MULTI_TARGET		0x40	// multiple target values will be adjusted
#define ADJUST_MULTI_OPERAND	0x80	// multiple operand values used for adjustments

// Conditional operation codes
#define COND_EXISTS				0x00	// Conditional: exists (non-zero value)
#define COND_NOT_EXISTS			0x01	// Conditional: NOT exists (zero value)
#define COND_EQUAL				0x02	// Conditional: equal
#define COND_NOT_EQUAL			0x03	// Conditional: not equal
#define COND_LESS				0x04	// Conditional: less than
#define COND_GREATER			0x05	// Conditional: greater than
#define COND_LESS_EQUAL			0x06	// Conditional: less than or equal
#define COND_GREATER_EQUAL		0x07	// Conditional: greater than or equal
#define COND_AND				0x08	// Conditional: AND
#define COND_OR					0x09	// Conditional: OR

// Conditional operation flags
#define COND_OP_MASK			0x0F	// conditional operation code mask
#define COND_ADVANCED_OFFSETS	0x10	// advanced offset values
#define COND_BUFFER_VALUE		0x20	// value to compare is a buffer-fetched value

// Reverse operation flags
#define REVERSE_16BIT			0x01	// 16-bit value length
#define REVERSE_32BIT			0x02	// 32-bit value length
#define REVERSE_SIZE			0x03	// when both length flags are set, a 16-bit length value follows
#define REVERSE_CHUNKED			0x04	// chunked reverse, 16-bit size value follows
#define REVERSE_BLOCK			0x08	// reverse block order
#define REVERSE_UNUSED_BITS		0xF0	// unused bits

// Buffered bitmap and sample info
#define BUFFERED_BITMAP_BASEID	0xFA00	// Base ID for buffered bitmaps
#define BUFFERED_SAMPLE_BASEID	0xFB00	// Base ID for buffered samples

// Viewport definitions
#define VIEWPORT_TEXT			0		// Text viewport
#define VIEWPORT_DEFAULT		1		// Default (whole screen) viewport
#define VIEWPORT_GRAPHICS		2		// Graphics viewport
#define VIEWPORT_ACTIVE			3		// Active viewport

#define LOGICAL_SCRW			1280	// As per the BBC Micro standard
#define LOGICAL_SCRH			1024

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE	0
#else
#define ARDUINO_RUNNING_CORE	1
#endif

// Function Prototypes
//
void debug_log(const char *format, ...);

// Additional modelines
//
#ifndef VGA_640x240_60Hz
#define VGA_640x240_60Hz	"\"640x240@60Hz\" 25.175 640 656 752 800 240 245 246 262 -HSync -VSync DoubleScan"
#endif

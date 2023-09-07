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
#define VDP_LOGICALCOORDS		0xC0	// Switch BBC Micro style logical coords on and off
#define VDP_LEGACYMODES			0xC1	// Switch VDP 1.03 compatible modes on and off
#define VDP_SWITCHBUFFER		0xC3	// Double buffering control
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

#define AUDIO_CHANNELS			3		// Default number of audio channels
#define MAX_AUDIO_CHANNELS		32		// Maximum number of audio channels
#define MAX_AUDIO_SAMPLES		128		// Maximum number of audio samples
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
#define AUDIO_WAVE_SAMPLE		8		// Sample playback (internally used, can't be passed as a parameter)
// negative values for waveforms indicate a sample number

#define AUDIO_SAMPLE_LOAD		0		// Send a sample to the VDP
#define AUDIO_SAMPLE_CLEAR		1		// Clear/delete a sample
#define AUDIO_SAMPLE_DEBUG_INFO 2		// Get debug info about a sample

#define AUDIO_ENVELOPE_NONE		0		// No envelope
#define AUDIO_ENVELOPE_ADSR		1		// Simple ADSR volume envelope

#define AUDIO_STATUS_ACTIVE		0x01	// Has an active waveform
#define AUDIO_STATUS_PLAYING	0x02	// Playing a note (not in release phase)
#define AUDIO_STATUS_INDEFINITE	0x04	// Indefinite duration sound playing
#define AUDIO_STATUS_HAS_VOLUME_ENVELOPE	0x08	// Channel has a volume envelope set
#define AUDIO_STATUS_HAS_FREQUENCY_ENVELOPE	0x10	// Channel has a frequency envelope set

#define AUDIO_STATE_IDLE		0		// Channel is idle/silent
#define AUDIO_STATE_PENDING		1		// Channel is pending (note will be played next loop call)
#define AUDIO_STATE_PLAYING		2		// Channel is playing a note (passive)
#define AUDIO_STATE_PLAY_LOOP	3		// Channel is in active note playing loop
#define AUDIO_STATE_RELEASE		4		// Channel is releasing a note
#define AUDIO_STATE_ABORT		5		// Channel is aborting a note

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
#define VGA_640x240_60Hz	"\"640x240@60Hz\" 25.175 640 656 752 800 240 245 247 262 -HSync -VSync DoubleScan"
#endif
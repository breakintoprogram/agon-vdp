//
// Title:	        Audio VDU command support
// Author:        	Steve Sims
// Created:       	29/07/2023
// Last Updated:	29/07/2023

#include <fabgl.h>

fabgl::SoundGenerator		SoundGenerator;		// The audio class

#include "agon.h"
#include "agon_audio.h"

extern void send_packet(byte code, byte len, byte data[]);
extern int readByte_t();
extern int readWord_t();

audio_channel *	audio_channels[AUDIO_CHANNELS];	// Storage for the channel data

TaskHandle_t audioHandlers[AUDIO_CHANNELS];		// Storage for audio handler task handlers

// Audio channel driver task
//
void audio_driver(void * parameters) {
	int channel = *(int *)parameters;

	audio_channels[channel] = new audio_channel(channel);
	while(true) {
		audio_channels[channel]->loop();
		vTaskDelay(1);
	}
}

void init_audio_channel(int channel) {
  	xTaskCreatePinnedToCore(audio_driver,  "audio_driver",
		4096,						// This stack size can be checked & adjusted by reading the Stack Highwater
		&channel,					// Parameters
		PLAY_SOUND_PRIORITY,		// Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
		&audioHandlers[channel],	// Task handle
		ARDUINO_RUNNING_CORE
	);
}

void audioTaskAbortDelay(int channel) {
	if(audioHandlers[channel] != NULL) {
		xTaskAbortDelay(audioHandlers[channel]);
	}
}

// Initialise the sound driver
//
void init_audio() {
	for(int i = 0; i < AUDIO_CHANNELS; i++) {
		init_audio_channel(i);
	}
	SoundGenerator.play(true);
}

// Send an audio acknowledgement
//
void sendPlayNote(int channel, int success) {
	byte packet[] = {
		channel,
		success,
	};
	send_packet(PACKET_AUDIO, sizeof packet, packet);
}

// Play a note
//
word play_note(byte channel, byte volume, word frequency, word duration) {
	if(channel >=0 && channel < AUDIO_CHANNELS) {
		return audio_channels[channel]->play_note(volume, frequency, duration);
	}
	return 1;
}

// Set channel waveform
//
void setWaveform(byte channel, byte waveformType) {
	if(channel >=0 && channel < AUDIO_CHANNELS) {
		audio_channels[channel]->setWaveform(waveformType);
	}
}

// Audio VDU command support (VDU 23, 0, &85, <args>)
//
void vdu_sys_audio() {
	int channel = readByte_t();		if(channel == -1) return;
	int command = readByte_t();		if(command == -1) return;

	switch (command) {
		case AUDIO_CMD_PLAY: {
			int volume = readByte_t();		if(volume == -1) return;
			int frequency = readWord_t();	if(frequency == -1) return;
			int duration = readWord_t();	if(duration == -1) return;

			sendPlayNote(channel, play_note(channel, volume, frequency, duration));
		}	break;
	
		case AUDIO_CMD_PLAYQUEUED: {
			debug_log("vdu_sys_audio: playqueued - not implemented yet\n\r");
		} 	break;

		case AUDIO_CMD_WAVEFORM: {
			int waveform = readByte_t();	if(waveform == -1) return;

			setWaveform(channel, waveform);
		}	break;

		case AUDIO_CMD_SAMPLE: {
			debug_log("vdu_sys_audio: sample - not implemented yet\n\r");
		}	break;

		case AUDIO_CMD_ENV_VOLUME: {
			debug_log("vdu_sys_audio: env_volume - not implemented yet\n\r");
		}	break;

		case AUDIO_CMD_ENV_FREQUENCY: {
			debug_log("vdu_sys_audio: env_frequency - not implemented yet\n\r");
		}	break;

		case AUDIO_CMD_RESET: {
			debug_log("vdu_sys_audio: reset - not implemented yet\n\r");
		}	break;
	}
}

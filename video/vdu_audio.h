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
extern int read24_t();
extern byte readByte_b();

audio_channel *	audio_channels[MAX_AUDIO_CHANNELS];	// Storage for the channel data
TaskHandle_t audioHandlers[MAX_AUDIO_CHANNELS];		// Storage for audio handler task handlers

audio_sample samples[MAX_AUDIO_SAMPLES];	// Storage for the sample data

// Audio channel driver task
//
void audio_driver(void * parameters) {
	int channel = *(int *)parameters;

	audio_channels[channel] = new audio_channel(channel);
	while (true) {
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
	if (audioHandlers[channel] != NULL) {
		xTaskAbortDelay(audioHandlers[channel]);
	}
}

void audioTaskKill(int channel) {
	if (audioHandlers[channel] != NULL) {
		vTaskDelete(audioHandlers[channel]);
		audioHandlers[channel] = NULL;
		delete audio_channels[channel];
		audio_channels[channel] = NULL;
	}
}

// Initialise the sound driver
//
void init_audio() {
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		audio_channels[i] = NULL;
		audioHandlers[i] = NULL;
	}
	for (int i = 0; i < AUDIO_CHANNELS; i++) {
		init_audio_channel(i);
	}
	SoundGenerator.play(true);
}

// Send an audio acknowledgement
//
void sendAudioStatus(int channel, int status) {
	byte packet[] = {
		channel,
		status,
	};
	send_packet(PACKET_AUDIO, sizeof packet, packet);
}

// Channel enabled?
//
bool channelEnabled(byte channel) {
	return channel >= 0 && channel < MAX_AUDIO_CHANNELS && audio_channels[channel] != NULL;
}

// Play a note
//
word play_note(byte channel, byte volume, word frequency, word duration) {
	if (channelEnabled(channel)) {
		return audio_channels[channel]->play_note(volume, frequency, duration);
	}
	return 1;
}

// Get channel status
//
byte getChannelStatus(byte channel) {
	if (channelEnabled(channel)) {
		return audio_channels[channel]->getStatus();
	}
	return -1;
}

// Set channel volume
//
void setVolume(byte channel, byte volume) {
	if (channelEnabled(channel)) {
		audio_channels[channel]->setVolume(volume);
	}
}

// Set channel frequency
//
void setFrequency(byte channel, word frequency) {
	if (channelEnabled(channel)) {
		audio_channels[channel]->setFrequency(frequency);
	}
}

// Set channel waveform
//
void setWaveform(byte channel, byte waveformType) {
	if (channelEnabled(channel)) {
		audio_channels[channel]->setWaveform(waveformType);
	}
}

void discardBytes(int length) {
	while (length > 0) {
		readByte_b();
		length--;
	}
}

// Clear a sample
//
byte clearSample(byte sample) {
	if (sample >= 0 && sample < MAX_AUDIO_SAMPLES) {
		// TODO stop any active playback of this sample
		free(samples[sample].data);
		samples[sample].data = NULL;
		samples[sample].length = 0;
		samples[sample].written = false;
		return 1;
	}
	return 0;
}

// Load a sample
//
byte loadSample(byte sample, int length) {
	debug_log("free mem: %d\n\r", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	if (sample >= 0 && sample < MAX_AUDIO_SAMPLES) {
		// Clear out existing sample
		clearSample(sample);

		// Allocate new sample
		int8_t * data = (int8_t *) heap_caps_malloc(length, MALLOC_CAP_SPIRAM);
		samples[sample].data = data;
		if (data != NULL) {
			// read data into buffer
			for (int n = 0; n < length; n++) data[n] = readByte_b();
			samples[sample].length = length;
			samples[sample].written = true;
			debug_log("vdu_sys_audio: sample %d - data loaded, length %d\n\r", sample, length);

			// test this sample out - temp
			// SamplesGenerator * waveform = new SamplesGenerator((const int8_t *)data, length);
			// SoundGenerator.attach(waveform);
			// waveform->enable(true);
			return 1;
		} else {
			// Failed to allocate memory
			// discard incoming serial data
			discardBytes(length);
			debug_log("vdu_sys_audio: sample %d - data discarded, no memory available length %d\n\r", sample, length);
			return 0;
		}
	}
	// Invalid sample number - discard incoming serial data
	discardBytes(length);
	debug_log("vdu_sys_audio: sample %d - data discarded, invalid sample number\n\r", sample);
	return 0;
}

// Set channel volume envelope
//
void setVolumeEnvelope(byte channel, byte type) {
	if (channelEnabled(channel)) {
		switch (type) {
			case AUDIO_ENVELOPE_NONE:
				audio_channels[channel]->setVolumeEnvelope(NULL);
				break;
			case AUDIO_ENVELOPE_ADSR:
				int attack = readWord_t();		if (attack == -1) return;
				int decay = readWord_t();		if (decay == -1) return;
				int sustain = readByte_t();		if (sustain == -1) return;
				int release = readWord_t();		if (release == -1) return;
				ADSRVolumeEnvelope *envelope = new ADSRVolumeEnvelope(attack, decay, sustain, release);
				audio_channels[channel]->setVolumeEnvelope(envelope);
				break;
		}
	}
}

// Audio VDU command support (VDU 23, 0, &85, <args>)
//
void vdu_sys_audio() {
	int channel = readByte_t();		if (channel == -1) return;
	int command = readByte_t();		if (command == -1) return;

	switch (command) {
		case AUDIO_CMD_PLAY: {
			int volume = readByte_t();		if (volume == -1) return;
			int frequency = readWord_t();	if (frequency == -1) return;
			int duration = readWord_t();	if (duration == -1) return;

			sendAudioStatus(channel, play_note(channel, volume, frequency, duration));
		}	break;

		case AUDIO_CMD_STATUS: {
			sendAudioStatus(channel, getChannelStatus(channel));
		}	break;

		case AUDIO_CMD_VOLUME: {
			int volume = readByte_t();		if (volume == -1) return;

			setVolume(channel, volume);
		}	break;

		case AUDIO_CMD_FREQUENCY: {
			int frequency = readWord_t();	if (frequency == -1) return;

			setFrequency(channel, frequency);
		}	break;

		case AUDIO_CMD_WAVEFORM: {
			int waveform = readByte_t();	if (waveform == -1) return;

			setWaveform(channel, waveform);
		}	break;

		case AUDIO_CMD_SAMPLE: {
			// channel number param re-purposed as sample number
			int action = readByte_t();		if (action == -1) return;

			switch (action) {
				case AUDIO_SAMPLE_LOAD: {
					// length as a 24-bit value
					int length = read24_t();		if (length == -1) return;

					sendAudioStatus(channel, loadSample(channel, length));
				}	break;

				case AUDIO_SAMPLE_CLEAR: {
					debug_log("vdu_sys_audio: clear sample %d\n\r", channel);
					sendAudioStatus(channel, clearSample(channel));
				}	break;

				default: {
					debug_log("vdu_sys_audio: unknown sample action %d\n\r", action);
					sendAudioStatus(channel, 0);
				}
			}
		}	break;

		case AUDIO_CMD_ENV_VOLUME: {
			int type = readByte_t();		if (type == -1) return;

			setVolumeEnvelope(channel, type);
		}	break;

		case AUDIO_CMD_ENV_FREQUENCY: {
			debug_log("vdu_sys_audio: env_frequency - not implemented yet\n\r");
		}	break;

		case AUDIO_CMD_ENABLE: {
			if (channel >= 0 && channel < MAX_AUDIO_CHANNELS && audio_channels[channel] == NULL) {
				init_audio_channel(channel);
			}
		}	break;

		case AUDIO_CMD_DISABLE: {
			if (channelEnabled(channel)) {
				audioTaskKill(channel);
			}
		}	break;

		case AUDIO_CMD_RESET: {
			if (channelEnabled(channel)) {
				audioTaskKill(channel);
				init_audio_channel(channel);
			}
		}	break;
	}
}

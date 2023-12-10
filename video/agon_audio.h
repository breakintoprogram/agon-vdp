//
// Title:			Agon Video BIOS - Audio class
// Author:			Dean Belfield
// Contributors:	Steve Sims (enhancements for more sophisticated audio support)
// Created:			05/09/2022
// Last Updated:	04/08/2023
//
// Modinfo:

#ifndef AGON_AUDIO_H
#define AGON_AUDIO_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <fabgl.h>

#include "audio_channel.h"
#include "audio_sample.h"
#include "types.h"

// audio channels and their associated tasks
std::unordered_map<uint8_t, std::shared_ptr<AudioChannel>> audioChannels;
std::vector<TaskHandle_t, psram_allocator<TaskHandle_t>> audioHandlers;

std::unordered_map<uint16_t, std::shared_ptr<AudioSample>> samples;	// Storage for the sample data

std::shared_ptr<fabgl::SoundGenerator> soundGenerator;				// audio handling sub-system

// Audio channel driver task
//
void audioDriver(void * parameters) {
	uint8_t channel = *(uint8_t *)parameters;

	audioChannels[channel] = make_shared_psram<AudioChannel>(channel);
	while (true) {
		audioChannels[channel]->loop();
		vTaskDelay(1);
	}
}

void initAudioChannel(uint8_t channel) {
	xTaskCreatePinnedToCore(audioDriver,  "audioDriver",
		4096,						// This stack size can be checked & adjusted by reading the Stack Highwater
		&channel,					// Parameters
		PLAY_SOUND_PRIORITY,		// Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
		&audioHandlers[channel],	// Task handle
		ARDUINO_RUNNING_CORE
	);
}

void audioTaskAbortDelay(uint8_t channel) {
	if (audioHandlers[channel]) {
		xTaskAbortDelay(audioHandlers[channel]);
	}
}

void audioTaskKill(uint8_t channel) {
	if (audioHandlers[channel]) {
		vTaskDelete(audioHandlers[channel]);
		audioHandlers[channel] = nullptr;
		audioChannels.erase(channel);
		debug_log("audioTaskKill: channel %d killed\n\r", channel);
	} else {
		debug_log("audioTaskKill: channel %d not found\n\r", channel);
	}
}

// Initialise the sound driver
//
void initAudio() {
	soundGenerator = std::make_shared<fabgl::SoundGenerator>();
	audioHandlers.reserve(MAX_AUDIO_CHANNELS);
	debug_log("initAudio: we have reserved %d channels\n\r", audioHandlers.capacity());
	for (uint8_t i = 0; i < AUDIO_CHANNELS; i++) {
		initAudioChannel(i);
	}
	soundGenerator->play(true);
}

// Channel enabled?
//
bool channelEnabled(uint8_t channel) {
	return channel < MAX_AUDIO_CHANNELS && audioChannels[channel];
}

// Play a note
//
uint8_t playNote(uint8_t channel, uint8_t volume, uint16_t frequency, uint16_t duration) {
	if (channelEnabled(channel)) {
		return audioChannels[channel]->playNote(volume, frequency, duration);
	}
	return 1;
}

// Get channel status
//
uint8_t getChannelStatus(uint8_t channel) {
	if (channelEnabled(channel)) {
		return audioChannels[channel]->getStatus();
	}
	return -1;
}

// Set channel volume
//
void setVolume(uint8_t channel, uint8_t volume) {
	if (channelEnabled(channel)) {
		audioChannels[channel]->setVolume(volume);
	}
}

// Set channel frequency
//
void setFrequency(uint8_t channel, uint16_t frequency) {
	if (channelEnabled(channel)) {
		audioChannels[channel]->setFrequency(frequency);
	}
}

// Set channel waveform
//
void setWaveform(uint8_t channel, int8_t waveformType, uint16_t sampleId) {
	if (channelEnabled(channel)) {
		auto channelRef = audioChannels[channel];
		channelRef->setWaveform(waveformType, channelRef, sampleId);
	}
}

// Clear a sample
//
uint8_t clearSample(uint16_t sampleId) {
	debug_log("clearSample: sample %d\n\r", sampleId);
	if (samples.find(sampleId) == samples.end()) {
		debug_log("clearSample: sample %d not found\n\r", sampleId);
		return 0;
	}
	samples.erase(sampleId);
	debug_log("reset sample\n\r");
	return 1;
}

// Reset samples
//
void resetSamples() {
	debug_log("resetSamples\n\r");
	samples.clear();
}

#endif // AGON_AUDIO_H

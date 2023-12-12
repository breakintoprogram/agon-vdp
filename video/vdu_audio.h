//
// Title:			Audio VDU command support
// Author:			Steve Sims
// Created:			29/07/2023
// Last Updated:	29/07/2023

#ifndef VDU_AUDIO_H
#define VDU_AUDIO_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <fabgl.h>

#include "agon.h"
#include "agon_audio.h"
#include "audio_sample.h"
#include "buffers.h"
#include "types.h"

// Audio VDU command support (VDU 23, 0, &85, <args>)
//
void VDUStreamProcessor::vdu_sys_audio() {
	auto channel = readByte_t();		if (channel == -1) return;
	auto command = readByte_t();		if (command == -1) return;

	switch (command) {
		case AUDIO_CMD_PLAY: {
			auto volume = readByte_t();		if (volume == -1) return;
			auto frequency = readWord_t();	if (frequency == -1) return;
			auto duration = readWord_t();	if (duration == -1) return;

			sendAudioStatus(channel, playNote(channel, volume, frequency, duration));
		}	break;

		case AUDIO_CMD_STATUS: {
			sendAudioStatus(channel, getChannelStatus(channel));
		}	break;

		case AUDIO_CMD_VOLUME: {
			auto volume = readByte_t();		if (volume == -1) return;

			setVolume(channel, volume);
		}	break;

		case AUDIO_CMD_FREQUENCY: {
			auto frequency = readWord_t();	if (frequency == -1) return;

			setFrequency(channel, frequency);
		}	break;

		case AUDIO_CMD_WAVEFORM: {
			auto waveform = readByte_t();	if (waveform == -1) return;
			auto sampleNum = 0;

			if (waveform == AUDIO_WAVE_SAMPLE) {
				// explit buffer number given for sample
				sampleNum = readWord_t();	if (sampleNum == -1) return;
			}

			// set waveform, interpretting waveform number as a signed 8-bit value
			// to allow for negative values to be used as sample numbers
			setWaveform(channel, (int8_t) waveform, sampleNum);
		}	break;

		case AUDIO_CMD_SAMPLE: {
			auto action = readByte_t();		if (action == -1) return;

			// sample number is negative 8 bit number, and provided in channel number param
			int16_t sampleNum = BUFFERED_SAMPLE_BASEID + (-(int8_t)channel - 1);	// convert to positive, ranged from zero

			switch (action) {
				case AUDIO_SAMPLE_LOAD: {
					// length as a 24-bit value
					auto length = read24_t();		if (length == -1) return;

					sendAudioStatus(channel, loadSample(sampleNum, length));
				}	break;

				case AUDIO_SAMPLE_CLEAR: {
					debug_log("vdu_sys_audio: clear sample %d\n\r", channel);
					sendAudioStatus(channel, clearSample(sampleNum));
				}	break;

				case AUDIO_SAMPLE_FROM_BUFFER: {
					auto bufferId = readWord_t();	if (bufferId == -1) return;
					auto format = readByte_t();		if (format == -1) return;
					uint32_t sampleRate = AUDIO_DEFAULT_SAMPLE_RATE;
					if (format & AUDIO_FORMAT_WITH_RATE) {
						sampleRate = readWord_t();	if (sampleRate == -1) return;
					}

					sendAudioStatus(channel, createSampleFromBuffer(bufferId, format, sampleRate));
				}	break;

				case AUDIO_SAMPLE_SET_FREQUENCY: {
					auto frequency = readWord_t();	if (frequency == -1) return;

					sendAudioStatus(channel, setSampleFrequency(sampleNum, frequency));
				}	break;

				case AUDIO_SAMPLE_BUFFER_SET_FREQUENCY: {
					auto bufferId = readWord_t();	if (bufferId == -1) return;
					auto frequency = readWord_t();	if (frequency == -1) return;

					sendAudioStatus(channel, setSampleFrequency(bufferId, frequency));
				}	break;

				case AUDIO_SAMPLE_SET_REPEAT_START: {
					auto repeatStart = read24_t();	if (repeatStart == -1) return;

					sendAudioStatus(channel, setSampleRepeatStart(sampleNum, repeatStart));
				}	break;

				case AUDIO_SAMPLE_BUFFER_SET_REPEAT_START: {
					auto bufferId = readWord_t();	if (bufferId == -1) return;
					auto repeatStart = read24_t();	if (repeatStart == -1) return;

					sendAudioStatus(channel, setSampleRepeatStart(bufferId, repeatStart));
				}	break;

				case AUDIO_SAMPLE_SET_REPEAT_LENGTH: {
					auto repeatLength = read24_t();	if (repeatLength == -1) return;

					sendAudioStatus(channel, setSampleRepeatLength(sampleNum, repeatLength));
				}	break;

				case AUDIO_SAMPLE_BUFFER_SET_REPEAT_LENGTH: {
					auto bufferId = readWord_t();	if (bufferId == -1) return;
					auto repeatLength = read24_t();	if (repeatLength == -1) return;

					sendAudioStatus(channel, setSampleRepeatLength(bufferId, repeatLength));
				}	break;

				case AUDIO_SAMPLE_DEBUG_INFO: {
					debug_log("Sample info: %d (%d)\n\r", (int8_t)channel, sampleNum);
					debug_log("  samples count: %d\n\r", samples.size());
					debug_log("  free mem: %d\n\r", heap_caps_get_free_size(MALLOC_CAP_8BIT));
					auto sample = samples[sampleNum];
					if (sample == nullptr) {
						debug_log("  sample is null\n\r");
						break;
					}
					auto buffer = sample->blocks;
					debug_log("  length: %d blocks\n\r", buffer.size());
					debug_log("  size: %d\n\r", sample->getSize());
					debug_log("  format: %d\n\r", sample->format);
					debug_log("  sample rate: %d\n\r", sample->sampleRate);
					debug_log("  base frequency: %d\n\r", sample->baseFrequency);
					debug_log("  repeat start: %d\n\r", sample->repeatStart);
					debug_log("  repeat length: %d\n\r", sample->repeatLength);
					if (buffer.size() > 0) {
						debug_log("  data first byte: %d\n\r", buffer[0]->getBuffer()[0]);
					}
				} break;

				default: {
					debug_log("vdu_sys_audio: unknown sample action %d\n\r", action);
					sendAudioStatus(channel, 0);
				}
			}
		}	break;

		case AUDIO_CMD_ENV_VOLUME: {
			auto type = readByte_t();		if (type == -1) return;

			setVolumeEnvelope(channel, type);
		}	break;

		case AUDIO_CMD_ENV_FREQUENCY: {
			auto type = readByte_t();		if (type == -1) return;

			setFrequencyEnvelope(channel, type);
		}	break;

		case AUDIO_CMD_ENABLE: {
			if (channel >= 0 && channel < MAX_AUDIO_CHANNELS && audioChannels[channel] == nullptr) {
				initAudioChannel(channel);
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
				initAudioChannel(channel);
			}
		}	break;

		case AUDIO_CMD_SEEK: {
			auto position = read24_t();		if (position == -1) return;

			if (channelEnabled(channel)) {
				audioChannels[channel]->seekTo(position);
			}
		}	break;

		case AUDIO_CMD_DURATION: {
			auto duration = read24_t();		if (duration == -1) return;

			if (channelEnabled(channel)) {
				audioChannels[channel]->setDuration(duration);
			}
		} break;

		case AUDIO_CMD_SAMPLERATE: {
			auto sampleRate = readWord_t();	if (sampleRate == -1) return;

			if (channel == 255) {
				// set underlying sample rate
				setSampleRate(sampleRate);
			} else if (channelEnabled(channel)) {
				audioChannels[channel]->setSampleRate(sampleRate);
			}
		}	break;
	}
}

// Send an audio acknowledgement
//
void VDUStreamProcessor::sendAudioStatus(uint8_t channel, uint8_t status) {
	uint8_t packet[] = {
		channel,
		status,
	};
	send_packet(PACKET_AUDIO, sizeof packet, packet);
}

// Load a sample
//
uint8_t VDUStreamProcessor::loadSample(uint16_t bufferId, uint32_t length) {
	debug_log("free mem: %d\n\r", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

	bufferClear(bufferId);

	if (bufferWrite(bufferId, length) != 0) {
		// timed out, or couldn't allocate buffer - so abort
		return 0;
	}
	return createSampleFromBuffer(bufferId, 0, AUDIO_DEFAULT_SAMPLE_RATE);
}

// Create a sample from a buffer
//
uint8_t VDUStreamProcessor::createSampleFromBuffer(uint16_t bufferId, uint8_t format, uint16_t sampleRate) {
	if (buffers.find(bufferId) == buffers.end()) {
		debug_log("vdu_sys_audio: buffer %d not found\n\r", bufferId);
		return 0;
	}
	clearSample(bufferId);
	auto sample = (format & AUDIO_FORMAT_WITH_RATE) ?
		make_shared_psram<AudioSample>(buffers[bufferId], format & AUDIO_FORMAT_DATA_MASK, sampleRate)
		: make_shared_psram<AudioSample>(buffers[bufferId], format & AUDIO_FORMAT_DATA_MASK);
	if (sample) {
		if (format & AUDIO_FORMAT_TUNEABLE) {
			sample->baseFrequency = AUDIO_DEFAULT_FREQUENCY;
		}
		samples[bufferId] = sample;
		return 1;
	}
	return 0;
}

// Set channel volume envelope
//
void VDUStreamProcessor::setVolumeEnvelope(uint8_t channel, uint8_t type) {
	if (channelEnabled(channel)) {
		switch (type) {
			case AUDIO_ENVELOPE_NONE:
				audioChannels[channel]->setVolumeEnvelope(nullptr);
				debug_log("vdu_sys_audio: channel %d - volume envelope disabled\n\r", channel);
				break;
			case AUDIO_ENVELOPE_ADSR:
				auto attack = readWord_t();		if (attack == -1) return;
				auto decay = readWord_t();		if (decay == -1) return;
				auto sustain = readByte_t();	if (sustain == -1) return;
				auto release = readWord_t();	if (release == -1) return;
				std::unique_ptr<VolumeEnvelope> envelope = make_unique_psram<ADSRVolumeEnvelope>(attack, decay, sustain, release);
				audioChannels[channel]->setVolumeEnvelope(std::move(envelope));
				break;
		}
	}
}

// Set channel frequency envelope
//
void VDUStreamProcessor::setFrequencyEnvelope(uint8_t channel, uint8_t type) {
	if (channelEnabled(channel)) {
		switch (type) {
			case AUDIO_ENVELOPE_NONE:
				audioChannels[channel]->setFrequencyEnvelope(nullptr);
				debug_log("vdu_sys_audio: channel %d - frequency envelope disabled\n\r", channel);
				break;
			case AUDIO_FREQUENCY_ENVELOPE_STEPPED:
				auto phaseCount = readByte_t();		if (phaseCount == -1) return;
				auto control = readByte_t();		if (control == -1) return;
				auto stepLength = readWord_t();		if (stepLength == -1) return;
				auto phases = make_shared_psram<std::vector<FrequencyStepPhase>>();
				for (auto n = 0; n < phaseCount; n++) {
					auto adjustment = readWord_t();	if (adjustment == -1) return;
					auto number = readWord_t();		if (number == -1) return;
					phases->push_back(FrequencyStepPhase { (int16_t)adjustment, (uint16_t)number });
				}
				bool repeats = control & AUDIO_FREQUENCY_REPEATS;
				bool cumulative = control & AUDIO_FREQUENCY_CUMULATIVE;
				bool restrict = control & AUDIO_FREQUENCY_RESTRICT;
				std::unique_ptr<FrequencyEnvelope> envelope = make_unique_psram<SteppedFrequencyEnvelope>(phases, stepLength, repeats, cumulative, restrict);
				audioChannels[channel]->setFrequencyEnvelope(std::move(envelope));
				break;
		}
	}
}

// Set sample frequency
//
uint8_t VDUStreamProcessor::setSampleFrequency(uint16_t sampleId, uint16_t frequency) {
	if (samples.find(sampleId) == samples.end()) {
		debug_log("vdu_sys_audio: sample %d not found\n\r", sampleId);
		return 0;
	}
	samples[sampleId]->baseFrequency = frequency;
	return 1;
}

// Set sample repeatStart
//
uint8_t VDUStreamProcessor::setSampleRepeatStart(uint16_t sampleId, uint32_t repeatStart) {
	if (samples.find(sampleId) == samples.end()) {
		debug_log("vdu_sys_audio: sample %d not found\n\r", sampleId);
		return 0;
	}
	samples[sampleId]->repeatStart = repeatStart;
	return 1;
}

// Set sample repeatLength
//
uint8_t VDUStreamProcessor::setSampleRepeatLength(uint16_t sampleId, uint32_t repeatLength) {
	if (samples.find(sampleId) == samples.end()) {
		debug_log("vdu_sys_audio: sample %d not found\n\r", sampleId);
		return 0;
	}
	if (repeatLength == 0xFFFFFF) {
		repeatLength = -1;
	}
	samples[sampleId]->repeatLength = repeatLength;
	return 1;
}

#endif // VDU_AUDIO_H

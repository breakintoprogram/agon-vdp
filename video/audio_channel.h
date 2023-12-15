#ifndef AUDIO_CHANNEL_H
#define AUDIO_CHANNEL_H

#include <memory>
#include <unordered_map>
#include <fabgl.h>

#include "agon.h"
#include "types.h"
#include "envelopes/volume.h"
#include "envelopes/frequency.h"

extern std::shared_ptr<fabgl::SoundGenerator> soundGenerator;	// audio handling sub-system
extern void audioTaskAbortDelay(uint8_t channel);

// The audio channel class
//
class AudioChannel {	
	public:
		AudioChannel(uint8_t channel);
		~AudioChannel();
		uint8_t		playNote(uint8_t volume, uint16_t frequency, int32_t duration);
		uint8_t		getStatus();
		std::unique_ptr<fabgl::WaveformGenerator>	getSampleWaveform(uint16_t sampleId, std::shared_ptr<AudioChannel> channelRef);
		void		setWaveform(int8_t waveformType, std::shared_ptr<AudioChannel> channelRef, uint16_t sampleId = 0);
		void		setVolume(uint8_t volume);
		void		setFrequency(uint16_t frequency);
		void		setVolumeEnvelope(std::unique_ptr<VolumeEnvelope> envelope);
		void		setFrequencyEnvelope(std::unique_ptr<FrequencyEnvelope> envelope);
		void		loop();
		uint8_t		channel() { return _channel; }
	private:
		void		waitForAbort();
		uint8_t		getVolume(uint32_t elapsed);
		uint16_t	getFrequency(uint32_t elapsed);
		bool		isReleasing(uint32_t elapsed);
		bool		isFinished(uint32_t elapsed);
		AudioState	_state;
		uint8_t		_channel;
		uint8_t		_volume;
		uint16_t	_frequency;
		int32_t		_duration;
		uint32_t	_startTime;
		uint8_t		_waveformType;
		std::unique_ptr<WaveformGenerator>	_waveform;
		std::unique_ptr<VolumeEnvelope>		_volumeEnvelope;
		std::unique_ptr<FrequencyEnvelope>	_frequencyEnvelope;
};

#include "audio_sample.h"
#include "enhanced_samples_generator.h"
extern std::unordered_map<uint16_t, std::shared_ptr<AudioSample>> samples;	// Storage for the sample data

AudioChannel::AudioChannel(uint8_t channel) {
	this->_channel = channel;
	this->_state = AudioState::Idle;
	this->_volume = 0;
	this->_frequency = 750;
	this->_duration = -1;
	setWaveform(AUDIO_WAVE_DEFAULT, nullptr);
	debug_log("AudioChannel: init %d\n\r", this->_channel);
	debug_log("free mem: %d\n\r", heap_caps_get_free_size(MALLOC_CAP_8BIT));
}

AudioChannel::~AudioChannel() {
	debug_log("AudioChannel: deiniting %d\n\r", this->_channel);
	if (this->_waveform) {
		this->_waveform->enable(false);
		soundGenerator->detach(this->_waveform.get());
	}
	debug_log("AudioChannel: deinit %d\n\r", this->_channel);
}

uint8_t AudioChannel::playNote(uint8_t volume, uint16_t frequency, int32_t duration) {
	switch (this->_state) {
		case AudioState::Idle:
		case AudioState::Release:
			this->_volume = volume;
			this->_frequency = frequency;
			this->_duration = duration == 65535 ? -1 : duration;
			if (this->_duration == 0 && this->_waveformType == AUDIO_WAVE_SAMPLE) {
				// zero duration means play whole sample
				this->_duration = ((EnhancedSamplesGenerator *)this->_waveform.get())->getDuration();
				if (this->_volumeEnvelope) {
					// subtract the "release" time from the duration
					this->_duration -= this->_volumeEnvelope->getRelease();
				}
				if (this->_duration < 0) {
					this->_duration = 1;
				}
			}
			this->_state = AudioState::Pending;
			debug_log("AudioChannel: playNote %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);
			return 1;
	}
	return 0;
}

uint8_t AudioChannel::getStatus() {
	uint8_t status = 0;
	if (this->_waveform && this->_waveform->enabled()) {
		status |= AUDIO_STATUS_ACTIVE;
		if (this->_duration == -1) {
			status |= AUDIO_STATUS_INDEFINITE;
		}
	}
	switch (this->_state) {
		case AudioState::Pending:
		case AudioState::Playing:
		case AudioState::PlayLoop:
			status |= AUDIO_STATUS_PLAYING;
			break;
	}
	if (this->_volumeEnvelope) {
		status |= AUDIO_STATUS_HAS_VOLUME_ENVELOPE;
	}
	if (this->_frequencyEnvelope) {
		status |= AUDIO_STATUS_HAS_FREQUENCY_ENVELOPE;
	}

	debug_log("AudioChannel: getStatus %d\n\r", status);
	return status;
}

std::unique_ptr<fabgl::WaveformGenerator> AudioChannel::getSampleWaveform(uint16_t sampleId, std::shared_ptr<AudioChannel> channelRef) {
	if (samples.find(sampleId) != samples.end()) {
		auto sample = samples.at(sampleId);
		// remove this channel from other samples
		for (auto samplePair : samples) {
			if (samplePair.second) {
				samplePair.second->channels.erase(_channel);
			}
		}
		sample->channels[_channel] = channelRef;

		return make_unique_psram<EnhancedSamplesGenerator>(sample);
	}
	return nullptr;
}

void AudioChannel::setWaveform(int8_t waveformType, std::shared_ptr<AudioChannel> channelRef, uint16_t sampleId) {
	std::unique_ptr<fabgl::WaveformGenerator> newWaveform = nullptr;

	switch (waveformType) {
		case AUDIO_WAVE_SAWTOOTH:
			newWaveform = make_unique_psram<SawtoothWaveformGenerator>();
			break;
		case AUDIO_WAVE_SQUARE:
			newWaveform = make_unique_psram<SquareWaveformGenerator>();
			break;
		case AUDIO_WAVE_SINE:
			newWaveform = make_unique_psram<SineWaveformGenerator>();
			break;
		case AUDIO_WAVE_TRIANGLE:
			newWaveform = make_unique_psram<TriangleWaveformGenerator>();
			break;
		case AUDIO_WAVE_NOISE:
			newWaveform = make_unique_psram<NoiseWaveformGenerator>();
			break;
		case AUDIO_WAVE_VICNOISE:
			newWaveform = make_unique_psram<VICNoiseGenerator>();
			break;
		case AUDIO_WAVE_SAMPLE:
			// Buffer-based sample playback
			debug_log("AudioChannel: using sample buffer %d for waveform\n\r", sampleId);
			newWaveform = getSampleWaveform(sampleId, channelRef);
			break;
		default:
			// negative values indicate a sample number
			if (waveformType < 0) {
				// convert our negative sample number to a positive sample number starting at our base buffer ID
				int16_t sampleNum = BUFFERED_SAMPLE_BASEID + (-waveformType - 1);
				debug_log("AudioChannel: using sample %d for waveform (%d)\n\r", waveformType, sampleNum);
				newWaveform = getSampleWaveform(sampleNum, channelRef);
				waveformType = AUDIO_WAVE_SAMPLE;
			} else {
				debug_log("AudioChannel: unknown waveform type %d\n\r", waveformType);
			}
			break;
	}

	if (newWaveform != nullptr) {
		debug_log("AudioChannel: setWaveform %d\n\r", waveformType);
		if (this->_state != AudioState::Idle) {
			debug_log("AudioChannel: aborting current playback\n\r");
			// some kind of playback is happening, so abort any current task delay to allow playback to end
			this->_state = AudioState::Abort;
			audioTaskAbortDelay(this->_channel);
			waitForAbort();
		}
		if (this->_waveform) {
			debug_log("AudioChannel: detaching old waveform\n\r");
			soundGenerator->detach(this->_waveform.get());
		}
		this->_waveform = std::move(newWaveform);
		_waveformType = waveformType;
		soundGenerator->attach(this->_waveform.get());
		debug_log("AudioChannel: setWaveform %d done\n\r", waveformType);
	}
}

void AudioChannel::setVolume(uint8_t volume) {
	debug_log("AudioChannel: setVolume %d\n\r", volume);

	if (this->_waveform) {
		waitForAbort();
		switch (this->_state) {
			case AudioState::Idle:
				if (volume > 0) {
					// new note playback
					this->_volume = volume;
					this->_duration = -1;	// indefinite duration
					this->_state = AudioState::Pending;
				}
				break;
			case AudioState::PlayLoop:
				// we are looping, so an envelope may be active
				if (volume == 0) {
					// silence whilst looping always stops playback - curtail duration
					this->_duration = millis() - this->_startTime;
					// if there's a volume envelope, just allow release to happen, otherwise...
					if (!this->_volumeEnvelope) {
						this->_volume = 0;
					}
				} else {
					// Change base volume level, so next loop iteration will use it
					this->_volume = volume;
				}
				break;
			case AudioState::Pending:
			case AudioState::Release:
				// Set level so next loop will pick up the new volume
				this->_volume = volume;
				break;
			default:
				// All other states we'll set volume immediately
				this->_volume = volume;
				this->_waveform->setVolume(volume);
				if (volume == 0) {
					// we're going silent, so abort any current playback
					this->_state = AudioState::Abort;
					audioTaskAbortDelay(this->_channel);
				}
				break;
		}	
	}
}

void AudioChannel::setFrequency(uint16_t frequency) {
	debug_log("AudioChannel: setFrequency %d\n\r", frequency);
	this->_frequency = frequency;

	if (this->_waveform) {
		waitForAbort();
		switch (this->_state) {
			case AudioState::Pending:
			case AudioState::PlayLoop:
			case AudioState::Release:
				// Do nothing as next loop will pick up the new frequency
				break;
			default:
				this->_waveform->setFrequency(frequency);
		}
	}
}

void AudioChannel::setVolumeEnvelope(std::unique_ptr<VolumeEnvelope> envelope) {
	this->_volumeEnvelope = std::move(envelope);
	if (envelope && this->_state == AudioState::Playing) {
		// swap to looping
		this->_state = AudioState::PlayLoop;
		audioTaskAbortDelay(this->_channel);
	}
}

void AudioChannel::setFrequencyEnvelope(std::unique_ptr<FrequencyEnvelope> envelope) {
	this->_frequencyEnvelope = std::move(envelope);
	if (envelope && this->_state == AudioState::Playing) {
		// swap to looping
		this->_state = AudioState::PlayLoop;
		audioTaskAbortDelay(this->_channel);
	}
}

void AudioChannel::waitForAbort() {
	while (this->_state == AudioState::Abort) {
		// wait for abort to complete
		vTaskDelay(1);
	}
}

uint8_t AudioChannel::getVolume(uint32_t elapsed) {
	if (this->_volumeEnvelope) {
		return this->_volumeEnvelope->getVolume(this->_volume, elapsed, this->_duration);
	}
	return this->_volume;
}

uint16_t AudioChannel::getFrequency(uint32_t elapsed) {
	if (this->_frequencyEnvelope) {
		return this->_frequencyEnvelope->getFrequency(this->_frequency, elapsed, this->_duration);
	}
	return this->_frequency;
}

bool AudioChannel::isReleasing(uint32_t elapsed) {
	if (this->_volumeEnvelope) {
		return this->_volumeEnvelope->isReleasing(elapsed, this->_duration);
	}
	if (this->_duration == -1) {
		return false;
	}
	return elapsed >= this->_duration;
}

bool AudioChannel::isFinished(uint32_t elapsed) {
	if (this->_volumeEnvelope) {
		return this->_volumeEnvelope->isFinished(elapsed, this->_duration);
	}
	if (this->_duration == -1) {
		return false;
	}
	return (elapsed >= this->_duration);
}

void AudioChannel::loop() {
	switch (this->_state) {
		case AudioState::Pending:
			debug_log("AudioChannel: play %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);
			// we have a new note to play
			this->_startTime = millis();
			// set our initial volume and frequency
			this->_waveform->setVolume(this->getVolume(0));
			if (this->_waveformType == AUDIO_WAVE_SAMPLE) {
				// hack to ensure samples always start from beginning
				this->_waveform->setFrequency(-1);
			}
			this->_waveform->setFrequency(this->getFrequency(0));
			this->_waveform->enable(true);
			// if we have an envelope then we loop, otherwise just delay for duration			
			if (this->_volumeEnvelope || this->_frequencyEnvelope) {
				this->_state = AudioState::PlayLoop;
			} else {
				this->_state = AudioState::Playing;
				// if delay value is negative then this delays for a super long time
				vTaskDelay(pdMS_TO_TICKS(this->_duration));
			}
			break;
		case AudioState::Playing:
			if (this->_duration >= 0) {
				// simple playback - delay until we have reached our duration
				uint32_t elapsed = millis() - this->_startTime;
				debug_log("AudioChannel: elapsed %d\n\r", elapsed);
				if (elapsed >= this->_duration) {
					this->_waveform->enable(false);
					debug_log("AudioChannel: end\n\r");
					this->_state = AudioState::Idle;
				} else {
					debug_log("AudioChannel: loop (%d)\n\r", this->_duration - elapsed);
					vTaskDelay(pdMS_TO_TICKS(this->_duration - elapsed));
				}
			} else {
				// our duration is indefinite, so delay for a long time
				debug_log("AudioChannel: loop (indefinite playback)\n\r");
				vTaskDelay(pdMS_TO_TICKS(-1));
			}
			break;
		// loop and release states used for envelopes
		case AudioState::PlayLoop: {
			uint32_t elapsed = millis() - this->_startTime;
			if (isReleasing(elapsed)) {
				debug_log("AudioChannel: releasing...\n\r");
				this->_state = AudioState::Release;
			}
			// update volume and frequency as appropriate
			if (this->_volumeEnvelope)
				this->_waveform->setVolume(this->getVolume(elapsed));
			if (this->_frequencyEnvelope)
				this->_waveform->setFrequency(this->getFrequency(elapsed));
			break;
		}
		case AudioState::Release: {
			uint32_t elapsed = millis() - this->_startTime;
			// update volume and frequency as appropriate
			if (this->_volumeEnvelope)
				this->_waveform->setVolume(this->getVolume(elapsed));
			if (this->_frequencyEnvelope)
				this->_waveform->setFrequency(this->getFrequency(elapsed));

			if (isFinished(elapsed)) {
				this->_waveform->enable(false);
				debug_log("AudioChannel: end (released)\n\r");
				this->_state = AudioState::Idle;
			}
			break;
		}
		case AudioState::Abort:
			this->_waveform->enable(false);
			debug_log("AudioChannel: abort\n\r");
			this->_state = AudioState::Idle;
			break;
	}
}

#endif // AUDIO_CHANNEL_H

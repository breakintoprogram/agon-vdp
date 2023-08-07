//
// Title:	        Agon Video BIOS - Audio class
// Author:        	Dean Belfield
// Contributors:	Steve Sims (enhancements for more sophisticated audio support)
// Created:       	05/09/2022
// Last Updated:	04/08/2023
//
// Modinfo:

#include <fabgl.h>

#include "envelopes.h"

extern void audioTaskAbortDelay(int channel);

// The audio channel class
//
class audio_channel {	
	public:
		audio_channel(int channel);
		~audio_channel();
		word	play_note(byte volume, word frequency, word duration);
		byte	getStatus();
		void	setWaveform(byte waveformType);
		void	setVolume(byte volume);
		void	setFrequency(word frequency);
		void	setVolumeEnvelope(VolumeEnvelope * envelope);
		void	loop();
	private:
		void	waitForAbort();
		byte	getVolume(word elapsed);
		word	getFrequency(word elapsed);
		bool	isReleasing(word elapsed);
		bool	isFinished(word elapsed);
		fabgl::WaveformGenerator *	_waveform;
		byte _waveformType;
		byte _state;
		byte _channel;
		byte _volume;
		word _frequency;
		long _duration;
		long _startTime;
		VolumeEnvelope * _volumeEnvelope;
};

audio_channel::audio_channel(int channel) {
	this->_channel = channel;
	this->_state = AUDIO_STATE_IDLE;
	this->_volume = 0;
	this->_frequency = 750;
	this->_duration = -1;
	this->_waveform = NULL;
	setWaveform(AUDIO_WAVE_DEFAULT);
	this->_volumeEnvelope = NULL;
	debug_log("audio_driver: init %d\n\r", this->_channel);
}

audio_channel::~audio_channel() {
	if (this->_waveform != NULL) {
		this->_waveform->enable(false);
		SoundGenerator.detach(this->_waveform);
		delete this->_waveform;
	}
	if (this->_volumeEnvelope != NULL) {
		delete this->_volumeEnvelope;
	}
	debug_log("audio_driver: deinit %d\n\r", this->_channel);
}

word audio_channel::play_note(byte volume, word frequency, word duration) {
	switch (this->_state) {
		case AUDIO_STATE_IDLE:
		case AUDIO_STATE_RELEASE:
			this->_volume = volume;
			this->_frequency = frequency;
			this->_duration = duration == 65535 ? -1 : duration;
			this->_state = AUDIO_STATE_PENDING;
			debug_log("audio_driver: play_note %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);
			return 1;
	}
	return 0;
}

byte audio_channel::getStatus() {
	byte status = 0;
	if (this->_waveform != NULL && this->_waveform->enabled()) {
		status |= AUDIO_STATUS_ACTIVE;
		if (this->_duration == -1) {
			status |= AUDIO_STATUS_INDEFINITE;
		}
	}
	switch (this->_state) {
		case AUDIO_STATE_PENDING:
		case AUDIO_STATE_PLAYING:
		case AUDIO_STATE_PLAY_LOOP:
			status |= AUDIO_STATUS_PLAYING;
			break;
	}
	if (this->_volumeEnvelope != NULL) {
		status |= AUDIO_STATUS_HAS_VOLUME_ENVELOPE;
	}
	// if (this->_frequencyEnvelope != NULL) {
	// 	status |= AUDIO_STATUS_HAS_FREQUENCY_ENVELOPE;
	// }

	debug_log("audio_driver: getStatus %d\n\r", status);
	return status;
}

void audio_channel::setWaveform(byte waveformType) {
	fabgl::WaveformGenerator * newWaveform = NULL;

	switch (waveformType) {
		case AUDIO_WAVE_SAWTOOTH:
			newWaveform = new SawtoothWaveformGenerator();
			break;
		case AUDIO_WAVE_SQUARE:
			newWaveform = new SquareWaveformGenerator();
			break;
		case AUDIO_WAVE_SINE:
			newWaveform = new SineWaveformGenerator();
			break;
		case AUDIO_WAVE_TRIANGLE:
			newWaveform = new TriangleWaveformGenerator();
			break;
		case AUDIO_WAVE_NOISE:
			newWaveform = new NoiseWaveformGenerator();
			break;
		case AUDIO_WAVE_VICNOISE:
			newWaveform = new VICNoiseGenerator();
			break;
		case AUDIO_WAVE_SAMPLE:
			debug_log("audio_driver: sample waveform not yet supported\n\r");
			break;
	}

	if (newWaveform != NULL) {
		debug_log("audio_driver: setWaveform %d\n\r", waveformType);
		if (this->_state != AUDIO_STATE_IDLE) {
			debug_log("audio_driver: aborting current playback\n\r");
			// some kind of playback is happening, so abort any current task delay to allow playback to end
			this->_state = AUDIO_STATE_ABORT;
			audioTaskAbortDelay(this->_channel);
			waitForAbort();
		}
		if (_waveform != NULL) {
			debug_log("audio_driver: detaching old waveform\n\r");
			SoundGenerator.detach(_waveform);
			delete _waveform;
		}
		_waveform = newWaveform;
		_waveformType = waveformType;
		debug_log("audio_driver: attaching new waveform\n\r");
		SoundGenerator.attach(_waveform);
		debug_log("audio_driver: setWaveform %d done\n\r", waveformType);
	}
}

void audio_channel::setVolume(byte volume) {
	debug_log("audio_driver: setVolume %d\n\r", volume);

	if (_waveform != NULL) {
		waitForAbort();
		switch (this->_state) {
			case AUDIO_STATE_IDLE:
				if (volume > 0) {
					// new note playback
					this->_volume = volume;
					this->_duration = -1;	// indefinite duration
					this->_state = AUDIO_STATE_PENDING;
				}
				break;
			case AUDIO_STATE_PLAY_LOOP:
				// we are looping, so an envelope may be active
				if (this->_volumeEnvelope != NULL && volume == 0 && this->_duration == -1) {
					// when we have an active envelope with indefinite playback and we're setting our volume to zero
					// we instead set the duration to the current elapsed time, so the envelope can finish (release)
					this->_duration = millis() - this->_startTime;
				} else {
					this->_volume = volume;
				}
				break;
			case AUDIO_STATE_PENDING:
			case AUDIO_STATE_RELEASE:
				// Set level so next loop will pick up the new volume
				this->_volume = volume;
				break;
			default:
				// All other states we'll set volume immediately
				this->_volume = volume;
				this->_waveform->setVolume(volume);
				if (volume == 0) {
					// we're going silent, so abort any current playback
					this->_state = AUDIO_STATE_ABORT;
					audioTaskAbortDelay(this->_channel);
				}
				break;
		}	
	}
}

void audio_channel::setFrequency(word frequency) {
	debug_log("audio_driver: setFrequency %d\n\r", frequency);
	this->_frequency = frequency;

	if (_waveform != NULL) {
		waitForAbort();
		switch (this->_state) {
			case AUDIO_STATE_PENDING:
			case AUDIO_STATE_PLAY_LOOP:
			case AUDIO_STATE_RELEASE:
				// Do nothing as next loop will pick up the new frequency
				break;
			default:
				this->_waveform->setFrequency(frequency);
		}
	}
}

void audio_channel::setVolumeEnvelope(VolumeEnvelope * envelope) {
	if (this->_volumeEnvelope != NULL) {
		delete this->_volumeEnvelope;
	}
	this->_volumeEnvelope = envelope;
	if (envelope != NULL && this->_state == AUDIO_STATE_PLAYING) {
		// swap to looping
		this->_state = AUDIO_STATE_PLAY_LOOP;
		audioTaskAbortDelay(this->_channel);
	}
}

void audio_channel::waitForAbort() {
	while (this->_state == AUDIO_STATE_ABORT) {
		// wait for abort to complete
		vTaskDelay(1);
	}
}

byte audio_channel::getVolume(word elapsed) {
	if (this->_volumeEnvelope != NULL) {
		return this->_volumeEnvelope->getVolume(this->_volume, elapsed, this->_duration);
	}
	return this->_volume;
}

word audio_channel::getFrequency(word elapsed) {
	// if (this->_frequencyEnvelope != NULL) {
	// 	return this->_frequencyEnvelope->getFrequency(this->_frequency, elapsed, this->_duration);
	// }
	return this->_frequency;
}

bool audio_channel::isReleasing(word elapsed) {
	if (this->_volumeEnvelope != NULL) {
		return this->_volumeEnvelope->isReleasing(elapsed, this->_duration);
	}
	return false;
}

bool audio_channel::isFinished(word elapsed) {
	if (this->_volumeEnvelope != NULL) {
		return this->_volumeEnvelope->isFinished(elapsed, this->_duration);
	}
	if (this->_duration == -1) {
		return false;
	}
	return (elapsed >= this->_duration);
}

void audio_channel::loop() {
	switch (this->_state) {
		case AUDIO_STATE_PENDING:
			debug_log("audio_driver: play %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);
			// we have a new note to play
			this->_startTime = millis();
			// set our initial volume and frequency
			this->_waveform->setVolume(this->getVolume(0));
			this->_waveform->setFrequency(this->getFrequency(0));
			this->_waveform->enable(true);
			// if we have an envelope then we loop, otherwise just delay for duration			
			if (this->_volumeEnvelope != NULL) {
				this->_state = AUDIO_STATE_PLAY_LOOP;
			} else {
				this->_state = AUDIO_STATE_PLAYING;
				// if delay value is negative then this delays for a super long time
				vTaskDelay(pdMS_TO_TICKS(this->_duration));
			}
			break;
		case AUDIO_STATE_PLAYING:
			if (this->_duration >= 0) {
				// simple playback - delay until we have reached our duration
				word elapsed = millis() - this->_startTime;
				debug_log("audio_driver: elapsed %d\n\r", elapsed);
				if (elapsed >= this->_duration) {
					this->_waveform->enable(false);
					debug_log("audio_driver: end\n\r");
					this->_state = AUDIO_STATE_IDLE;
				} else {
					debug_log("audio_driver: loop (%d)\n\r", this->_duration - elapsed);
					vTaskDelay(pdMS_TO_TICKS(this->_duration - elapsed));
				}
			} else {
				// our duration is indefinite, so delay for a long time
				debug_log("audio_driver: loop (indefinite playback)\n\r");
				vTaskDelay(pdMS_TO_TICKS(-1));
			}
			break;
		// loop and release states used for envelopes
		case AUDIO_STATE_PLAY_LOOP: {
			word elapsed = millis() - this->_startTime;
			if (isReleasing(elapsed)) {
				debug_log("audio_driver: releasing...\n\r");
				this->_state = AUDIO_STATE_RELEASE;
			}
			// update volume and frequency as appropriate
			this->_waveform->setVolume(this->getVolume(elapsed));
			break;
		}
		case AUDIO_STATE_RELEASE: {
			word elapsed = millis() - this->_startTime;
			byte newVolume = this->getVolume(elapsed);

			if (isFinished(elapsed)) {
				// we've reached zero volume, so stop playback
				this->_waveform->enable(false);
				debug_log("audio_driver: end (released)\n\r");
				this->_state = AUDIO_STATE_IDLE;
			} else {
				// update volume
				this->_waveform->setVolume(newVolume);
			}
			break;
		}
		case AUDIO_STATE_ABORT:
			this->_waveform->enable(false);
			debug_log("audio_driver: abort\n\r");
			this->_state = AUDIO_STATE_IDLE;
			break;
	}
}

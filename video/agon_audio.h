//
// Title:	        Agon Video BIOS - Audio class
// Author:        	Dean Belfield
// Contributors:	Steve Sims (enhancements for more sophisticated audio support)
// Created:       	05/09/2022
// Last Updated:	04/08/2023
//
// Modinfo:

#include <fabgl.h>

extern void audioTaskAbortDelay(int channel);

// The audio channel class
//
class audio_channel {	
	public:
		audio_channel(int channel);
		word	play_note(byte volume, word frequency, word duration);
		byte	getStatus();
		void	setWaveform(byte waveformType);
		void	setVolume(byte volume);
		void	setFrequency(word frequency);
		void	loop();
	private:
		void	waitForAbort();
		fabgl::WaveformGenerator *	_waveform;
		byte _waveformType;
		byte _state;
		byte _channel;
		byte _volume;
		word _frequency;
		word _duration;
		long _startTime;
};

audio_channel::audio_channel(int channel) {
	this->_channel = channel;
	this->_state = AUDIO_STATE_IDLE;
	this->_volume = 0;
	this->_frequency = 750;
	this->_duration = 0;
	this->_waveform = NULL;
	setWaveform(AUDIO_WAVE_DEFAULT);
	debug_log("audio_driver: init %d\n\r", this->_channel);
}

word audio_channel::play_note(byte volume, word frequency, word duration) {
	switch (this->_state) {
		case AUDIO_STATE_IDLE:
		case AUDIO_STATE_RELEASE:
			this->_volume = volume;
			this->_frequency = frequency;
			this->_duration = duration;
			this->_state = AUDIO_STATE_PENDING;
			debug_log("audio_driver: play_note %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);
			return 1;
	}
	return 0;
}

byte audio_channel::getStatus() {
	debug_log("audio_driver: getStatus %d\n\r", this->_state);
	// TODO replace with bitfield based status
	return this->_state;
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
	this->_volume = volume;

	if (_waveform != NULL) {
		waitForAbort();
		switch (this->_state) {
			case AUDIO_STATE_IDLE:
				if (volume > 0) {
					// new note playback
					this->_duration = 0;
					this->_state = AUDIO_STATE_PENDING;
				}
				break;
			case AUDIO_STATE_PENDING:
			case AUDIO_STATE_PLAY_LOOP:
			case AUDIO_STATE_RELEASE:
				// Do nothing as next loop will pick up the new volume
				break;
			default:
				// All other states we'll set volume immediately
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

void audio_channel::waitForAbort() {
	while (this->_state == AUDIO_STATE_ABORT) {
		// wait for abort to complete
		vTaskDelay(1);
	}
}

void audio_channel::loop() {
	switch (this->_state) {
		case AUDIO_STATE_PENDING:
			debug_log("audio_driver: play %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);
			// we have a new note to play
			this->_startTime = millis();
			// set our initial volume and frequency
			// TODO use getVolume and getFrequency
			this->_waveform->setVolume(this->_volume);
			this->_waveform->setFrequency(this->_frequency);
			this->_waveform->enable(true);
			// if we have an envelope then we loop, otherwise just delay for duration			
			this->_state = AUDIO_STATE_PLAYING;
			// delay for 1ms less than our duration, as each loop costs 1ms
			// if delay value is -1 then this delays for a super long time
			vTaskDelay(pdMS_TO_TICKS(this->_duration - 1));
			break;
		case AUDIO_STATE_PLAYING:
			if (this->_duration > 0) {
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
		// neither of these states can currently be reached
		case AUDIO_STATE_PLAY_LOOP:
			// has our elapsed time gone past our duration?
			// drop thru to release
		case AUDIO_STATE_RELEASE:
			// update volume and frequency as appropriate
			// do we need to change state?
			break;
		case AUDIO_STATE_ABORT:
			this->_waveform->enable(false);
			debug_log("audio_driver: abort\n\r");
			this->_state = AUDIO_STATE_IDLE;
			break;
	}
}

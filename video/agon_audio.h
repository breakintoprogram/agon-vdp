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
		void	loop();
	private:
		fabgl::WaveformGenerator *	_waveform;
		byte _waveformType;
		byte _state;
		byte _channel;
		byte _volume;
		word _frequency;
		word _duration;
};

audio_channel::audio_channel(int channel) {
	this->_channel = channel;
	this->_state = AUDIO_STATUS_SILENT;
	this->_waveform = NULL;
	setWaveform(AUDIO_WAVE_DEFAULT);
	debug_log("audio_driver: init %d\n\r", this->_channel);
}

word audio_channel::play_note(byte volume, word frequency, word duration) {
	if (this->_state != AUDIO_STATUS_PLAYING) {
		this->_volume = volume;
		this->_frequency = frequency;
		this->_duration = duration;
		this->_state = AUDIO_STATUS_PLAYING;
		debug_log("audio_driver: play_note %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);
		return 1;
	}
	return 0;
}

byte audio_channel::getStatus() {
	debug_log("audio_driver: getStatus %d\n\r", this->_state);
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
		if (this->_state != AUDIO_STATUS_SILENT) {
			debug_log("audio_driver: aborting current playback\n\r");
			// playback is happening, so abort any current task delay to allow playback to end
			this->_state = AUDIO_STATUS_ABORT;
			audioTaskAbortDelay(this->_channel);
			// delay here to allow loop to abort playback
			vTaskDelay(1);
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

	// TODO what to do about duration??
	// currently a silent channel that had a duration will play a new note of previous duration

	if (_waveform != NULL) {
		switch(this->_state) {
			case AUDIO_STATUS_SILENT:
				if (volume > 0) {
					this->_state = AUDIO_STATUS_PLAYING;
					this->_waveform->setVolume(volume);
					if (!this->_waveform->enabled()) {
						this->_waveform->enable(true);
					}
				}
				break;
			case AUDIO_STATUS_RELEASE:
				// Release handled same as playing
				// but we need to ensure that we remove duration
			case AUDIO_STATUS_PLAYING:
				this->_waveform->setVolume(volume);
				if (volume == 0) {
					this->_state = AUDIO_STATUS_ABORT;
					audioTaskAbortDelay(this->_channel);
				}
				break;
			case AUDIO_STATUS_ABORT:
				// nothing to do
				// we are already aborting playback, so don't change the volume
				break;
		}	
	}
}

void audio_channel::loop() {
	// TODO handle duration-less audio

	// TODO consider status handling a bit deeper here
	// once we support envelopes the loop will need to be more complex
	// and better understand our true state.
	if (this->_state != AUDIO_STATUS_SILENT) {
		debug_log("audio_driver: play %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);
		this->_waveform->setVolume(this->_volume);
		this->_waveform->setFrequency(this->_frequency);
		this->_waveform->enable(true);
		vTaskDelay(pdMS_TO_TICKS(this->_duration));
		this->_waveform->enable(false);
		debug_log("audio_driver: end\n\r");
		this->_state = AUDIO_STATUS_SILENT;
	}
}

//
// Title:	        Agon Video BIOS - Audio class
// Author:        	Dean Belfield
// Created:       	05/09/2022
// Last Updated:	05/09/2022
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
		void	setWaveform(byte waveformType);
		void	loop();
	private:
		fabgl::WaveformGenerator *	_waveform;
		byte _waveformType;
	 	byte _flag;	// TODO replace with state
		byte _channel;
		byte _volume;
		word _frequency;
		word _duration;
};

audio_channel::audio_channel(int channel) {
	this->_channel = channel;
	this->_flag = 0;
	this->_waveform = NULL;
	setWaveform(AUDIO_WAVE_DEFAULT);
	debug_log("audio_driver: init %d\n\r", this->_channel);
}

word audio_channel::play_note(byte volume, word frequency, word duration) {
	if(this->_flag == 0) {
		this->_volume = volume;
		this->_frequency = frequency;
		this->_duration = duration;
		this->_flag++;
		debug_log("audio_driver: play_note %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);
		return 1;	
	}
	return 0;
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
		// TODO use state instead of flag
		if (this->_flag > 0) {
			debug_log("audio_driver: aborting current playback\n\r");
			// TODO set state to "aborting"
			// playback is happening, so abort any current task delay to allow playback to end
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

void audio_channel::loop() {
	if(this->_flag > 0) {
		debug_log("audio_driver: play %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);
		this->_waveform->setVolume(this->_volume);
		this->_waveform->setFrequency(this->_frequency);
		this->_waveform->enable(true);
		vTaskDelay(pdMS_TO_TICKS(this->_duration));
		this->_waveform->enable(false);
		debug_log("audio_driver: end\n\r");
		this->_flag = 0;
	}
}

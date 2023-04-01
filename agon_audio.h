//
// Title:	        Agon Video BIOS - Audio class
// Author:        	Dean Belfield
// Created:       	05/09/2022
// Last Updated:	05/09/2022
//
// Modinfo:

#pragma once

// The audio channel class
//
class audio_channel {	
	public:
		audio_channel(int channel);		
		word	play_note(byte volume, word frequency, word duration);
    word  play_note(byte volume, word frequency, word duration, byte wave);
    word  play_note(byte volume, word frequency, word duration, byte wave, word attack, word decay, byte sustain, word release, word frequency_end, byte end_style);
		void	loop();
	private:
		fabgl::WaveformGenerator *	_waveform;			
	 	byte _flag;
    byte _wavetype;
    
    byte _complexity;

		byte _channel;
		byte _volume; //Peak volume
    word _duration;
    
    word _attack;
    word _decay;
    byte _sustain; //Sustained volume
    word _release;
		word _frequency;
    word _frequency_end;
    byte _end_style;

};

audio_channel::audio_channel(int channel) {
	this->_channel = channel;
	this->_flag = 0;
	this->_waveform = new SawtoothWaveformGenerator();
	SoundGenerator.attach(_waveform);
	SoundGenerator.play(true);
	debug_log("audio_driver: init %d\n\r", this->_channel);			
}

word audio_channel::play_note(byte volume, word frequency, word duration, byte wave, word attack, word decay, byte sustain, word release, word frequency_end, byte end_style) { //Custom wave type *and* ASDR envelope
	if(this->_flag == 0) {

    this->_complexity = 2;

    SoundGenerator.detach(_waveform);
    debug_log("Detached old waveform\n\r");
    if (wave == 0)    _waveform = new SquareWaveformGenerator();
    if (wave == 1)    _waveform = new SineWaveformGenerator();
    if (wave == 2)    _waveform = new TriangleWaveformGenerator();
    if (wave == 3)    _waveform = new SawtoothWaveformGenerator();
    if (wave == 4)    _waveform = new NoiseWaveformGenerator();
    SoundGenerator.attach(_waveform);
    debug_log("Attached new waveform\n\r");
    this->_volume = volume;
		this->_frequency = frequency;
		this->_duration = duration;

    this->_attack = attack;
    this->_decay = decay;
    this->_sustain = sustain;
    this->_release = release;
    
    this->_frequency_end = frequency_end;
    this->_end_style = end_style;

		this->_flag++;
		debug_log("audio_driver: play_note %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);		
		return 1;	
	}
	return 0;
}

word audio_channel::play_note(byte volume, word frequency, word duration, byte wave) { //Simple but with custom wave type
	if(this->_flag == 0) {
    
    this->_complexity = 1;
    
    SoundGenerator.detach(_waveform);
    debug_log("Detached old waveform\n\r");
    if (wave == 0)    _waveform = new SquareWaveformGenerator();
    if (wave == 1)    _waveform = new SineWaveformGenerator();
    if (wave == 2)    _waveform = new TriangleWaveformGenerator();
    if (wave == 3)    _waveform = new SawtoothWaveformGenerator();
    if (wave == 4)    _waveform = new NoiseWaveformGenerator();
    SoundGenerator.attach(_waveform);
    debug_log("Attached new waveform\n\r");
    this->_volume = volume;
		this->_frequency = frequency;
		this->_duration = duration;
		this->_flag++;
		debug_log("audio_driver: play_note %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);		
		return 1;	
	}
	return 0;
}

word audio_channel::play_note(byte volume, word frequency, word duration) { //Simple (default saw wave)
	if(this->_flag == 0) {
		
    this->_complexity = 0;

    SoundGenerator.detach(_waveform);
    this->_waveform = new SawtoothWaveformGenerator();
    SoundGenerator.attach(_waveform);
    this->_volume = volume;
		this->_frequency = frequency;
		this->_duration = duration;
		this->_flag++;
		debug_log("audio_driver: play_note %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);		
		return 1;	
	}
	return 0;
}

void audio_channel::loop() {
	if(this->_flag > 0) {
		debug_log("audio_driver: play %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);			

    if (this->_complexity == 2) {

      int sustainVolume = this->_sustain * this->_volume / 127;
      this->_waveform->setVolume( ((this->_attack == 0) ? ( (this->_decay != 0) ? this->_volume : sustainVolume) : 0) );
      this->_waveform->setFrequency(this->_frequency);
      this->_waveform->enable(true);

      long startTime = millis();

      while ( millis() - startTime < this->_duration) {

        long current = millis() - startTime;

        if ( current < this->_attack ) // Rising (Attack)
          this->_waveform->setVolume( (this->_volume * current) / this->_attack );
        else if ( current > this->_attack && current < (this->_attack + this->_decay)) // Fading to norm (Decay)
          this->_waveform->setVolume( map( current - this->_attack,  0, this->_decay,  this->_volume, sustainVolume ) );
        else if ( current > this->_duration - this->_release) // Fading to end (Release)
          this->_waveform->setVolume( map( current - (this->_duration - this->_release),  0, this->_release,  sustainVolume, 0 ) );
        else
          this->_waveform->setVolume(sustainVolume); //Keep at norm (Sustain)

      if ( this->_end_style != 0) {
          int maxtime = this->_duration;
          if (this->_end_style == 2)
            maxtime = (this->_duration - this->_release); // until release
          else if (this->_end_style == 3)
            maxtime = (this->_attack + this->_decay); // until sustain

          int f = ((current > maxtime) ? this->_frequency_end : (map(current, 0, maxtime, this->_frequency, this->_frequency_end)));
          this->_waveform->setFrequency(f) ;
      }


      vTaskDelay(1);

      }

    } else { // No envelope

      this->_waveform->setVolume(this->_volume);
      this->_waveform->setFrequency(this->_frequency);
      this->_waveform->enable(true);
      if (this->_complexity == 0) vTaskDelay(this->_duration);
      else vTaskDelay(pdMS_TO_TICKS(this->_duration));
    
    }
		
    this->_waveform->enable(false);
    debug_log("audio_driver: end\n\r");			
		this->_flag = 0; 

	}
}

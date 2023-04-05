//
// Title:	        Agon Video BIOS - Audio class
// Author:        	Dean Belfield
// Created:       	05/09/2022
// Last Updated:	05/09/2022
//
// Modinfo:

#pragma once

extern void sendChannelStatus(int channel, int status);

#define SAMPLE_SLOTS 6

struct audio_sample
{
    
    bool written = false;         // has this sample been written yet? If note, ignore efforts to play.
    uint16_t length;              // sample length
    uint16_t rate;                // sample rate
    int8_t sample_buffer[262144]; // Max 256KB of raw PCM

};

struct audio_sample* samples[SAMPLE_SLOTS]; //6 Samples = ~1.5MB of PSRAM

// The audio channel class
//
class audio_channel {	
	public:
		audio_channel(int channel);		
		word	play_note_simple(byte volume, word frequency, word duration, byte override);
    word  play_note_cwave(byte volume, word frequency, word duration, byte wave, byte override);
    word  play_wave(byte sample_id, byte volume);
    word  play_note_env(byte volume, word frequency, word duration, byte wave, word attack, word decay, byte sustain, word release, word frequency_end, byte end_style, byte override);
		void	loop();
	private:
		fabgl::WaveformGenerator *	_waveform;
    
    float _s_duration_ms = 0;

	 	byte _flag;
    byte _wavetype;
    
    byte _complexity;

		byte _channel;
		byte _volume; //Peak volume
    word _duration;
    word _sample_rate;
    
    word _attack;
    word _decay;
    byte _sustain; //Sustained volume
    word _release;
		word _frequency;
    word _frequency_end;
    byte _end_style;

    byte _override = 0;

};

audio_channel::audio_channel(int channel) {
	this->_channel = channel;
	this->_flag = 0;
	this->_waveform = new SawtoothWaveformGenerator();
	SoundGenerator.attach(_waveform);
	SoundGenerator.play(true);
	debug_log("audio_driver: init %d\n\r", this->_channel);			
}

word audio_channel::play_note_env(byte volume, word frequency, word duration, byte wave, word attack, word decay, byte sustain, word release, word frequency_end, byte end_style, byte b_override = 0) { //Custom wave type *and* ASDR envelope
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

    this->_override = b_override;

		this->_flag++;
		debug_log("audio_driver: play_note %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);		
		return 1;	
	}
	return 0;
}

word audio_channel::play_note_cwave(byte volume, word frequency, word duration, byte wave, byte b_override = 0) { //Simple but with custom wave type
	if(this->_flag == 0) {
    
    if (b_override == 0) this->_complexity = 1;
    else if (b_override > 0) this->_complexity = 4;
    
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
		
    this->_override = b_override;
    this->_flag++;
		
    debug_log("audio_driver: play_note %d,%d,%d,%d\n\r", this->_channel, this->_volume, this->_frequency, this->_duration);		
		return 1;	
	}
	return 0;
}

word audio_channel::play_wave(byte sample_id, byte volume) { //Simple PCM
	if(this->_flag == 0) {
    
    this->_complexity = 5;
    this->_volume = volume;
    this->_duration = samples[sample_id]->length;
    this->_sample_rate = samples[sample_id]->rate;

    SoundGenerator.detach(this->_waveform);
    free(this->_waveform);
    this->_waveform = new SamplesGenerator(samples[sample_id]->sample_buffer, sizeof(samples[sample_id]->sample_buffer));
    this->_s_duration_ms = ((float)this->_duration / (float)this->_sample_rate) * 1000;
    SoundGenerator.attach(this->_waveform);
    this->_override = 0;
    this->_flag++;

		return 1;	
	}
	return 0;
}

word audio_channel::play_note_simple(byte volume, word frequency, word duration, byte override = 0) { //Simple (default saw wave)
	if(this->_flag == 0 || override > 0) {
		
    if (override == 0) this->_complexity = 0;
    else if (override == 1) this->_complexity = 3;

    SoundGenerator.detach(this->_waveform);
    this->_waveform = new SawtoothWaveformGenerator();
    SoundGenerator.attach(this->_waveform);
    this->_volume = volume;
		this->_frequency = frequency;
		this->_duration = duration;

		this->_override = override;
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
    } else if (this->_complexity == 5) { //PCM Audio

      this->_waveform->setVolume(this->_volume);
      this->_waveform->setDuration(this->_duration);
      //this->_waveform->setSampleRate(8000);
      this->_waveform->enable(true);
      // this->_waveform->setAutoDetach(true);
      // this->_waveform->setAutoDestroy(true);
      vTaskDelay(pdMS_TO_TICKS((int)this->_s_duration_ms));

    }
    else { // No envelope

      this->_waveform->setVolume(this->_volume);
      this->_waveform->setFrequency(this->_frequency);
      this->_waveform->enable(true);
      if (this->_complexity == 0) vTaskDelay(this->_duration);
      else if (this->_complexity == 1) vTaskDelay(pdMS_TO_TICKS(this->_duration));
    
    }
		

    if (this->_override == 0) { //Complexity 3 and 4 are complexity 0 and 1 with infinite duration until overriden.
      this->_waveform->enable(false);
      SoundGenerator.detach(this->_waveform);
      sendChannelStatus(this->_channel, 0);
      debug_log("audio_driver: end\n\r");			
    }
    
    this->_flag = 0;

	}
}

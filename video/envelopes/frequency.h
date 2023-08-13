//
// Title:	        Audio Frequency Envelope support
// Author:        	Steve Sims
// Created:       	13/08/2023
// Last Updated:	13/08/2023

#include <memory>
#include <vector>

class FrequencyEnvelope {
	public:
		virtual uint16_t getFrequency(uint16_t baseFrequency, uint16_t elapsed, long duration);
		virtual bool isFinished(uint16_t elapsed, long duration);
};

struct FrequencyStep {
	int16_t increment;		// change of frequency per step
	uint16_t number;		// number of steps
};

class SteppedFrequencyEnvelope : public FrequencyEnvelope {
	public:
		SteppedFrequencyEnvelope(std::shared_ptr<std::vector<FrequencyStep>> steps, uint16_t stepLength, bool repeats);
		uint16_t getFrequency(uint16_t baseFrequency, uint16_t elapsed, long duration);
		bool isFinished(uint16_t elapsed, long duration);
	private:
		std::shared_ptr<std::vector<FrequencyStep>> _steps;
		long _stepLength;
		long _totalLength;
		bool _repeats;
};

SteppedFrequencyEnvelope::SteppedFrequencyEnvelope(std::shared_ptr<std::vector<FrequencyStep>> steps, uint16_t stepLength, bool repeats)
	: _steps(steps), _stepLength(stepLength), _repeats(repeats)
{
	_totalLength = 0;
	for (auto step : *this->_steps) {
		_totalLength += step.number * this->_stepLength;
	}

	debug_log("audio_driver: SteppedFrequencyEnvelope: stepLength=%d, repeats=%d, totalLength=%d\n\r", this->_stepLength, this->_repeats, _totalLength);
}

uint16_t SteppedFrequencyEnvelope::getFrequency(uint16_t baseFrequency, uint16_t elapsed, long duration) {
	// returns frequency for the given elapsed time
	// a duration of -1 means we're playing forever
	
	return baseFrequency;
}

bool SteppedFrequencyEnvelope::isFinished(uint16_t elapsed, long duration) {
	if (_repeats) {
		// a repeating frequency envelope never finishes
		return false;
	}

	return elapsed >= _totalLength;
}

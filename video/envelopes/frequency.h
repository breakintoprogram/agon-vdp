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

struct FrequencyStepPhase {
	int16_t adjustment;		// change of frequency per step
	uint16_t number;		// number of steps
};

class SteppedFrequencyEnvelope : public FrequencyEnvelope {
	public:
		SteppedFrequencyEnvelope(std::shared_ptr<std::vector<FrequencyStepPhase>> phases, uint16_t stepLength, bool repeats);
		uint16_t getFrequency(uint16_t baseFrequency, uint16_t elapsed, long duration);
		bool isFinished(uint16_t elapsed, long duration);
	private:
		std::shared_ptr<std::vector<FrequencyStepPhase>> _phases;
		long _stepLength;
		int _totalSteps;
		int _totalAdjustment;
		int _totalLength;
		bool _repeats;
};

SteppedFrequencyEnvelope::SteppedFrequencyEnvelope(std::shared_ptr<std::vector<FrequencyStepPhase>> phases, uint16_t stepLength, bool repeats)
	: _phases(phases), _stepLength(stepLength), _repeats(repeats)
{
	_totalSteps = 0;
	_totalLength = 0;
	_totalAdjustment = 0;

	for (auto phase : *this->_phases) {
		_totalSteps += phase.number;
		_totalLength += phase.number * _stepLength;
		_totalAdjustment += (phase.number * phase.adjustment);
	}

	debug_log("audio_driver: SteppedFrequencyEnvelope: totalSteps=%d, totalAdjustment=%d\n\r", this->_totalSteps, this->_totalAdjustment);
	debug_log("audio_driver: SteppedFrequencyEnvelope: stepLength=%d, repeats=%d, totalLength=%d\n\r", this->_stepLength, this->_repeats, _totalLength);
}

uint16_t SteppedFrequencyEnvelope::getFrequency(uint16_t baseFrequency, uint16_t elapsed, long duration) {
	// returns frequency for the given elapsed time
	// a duration of -1 means we're playing forever
	int currentStep = (elapsed / this->_stepLength) % this->_totalSteps;
	int loopCount = elapsed / this->_totalLength;

	if (!_repeats && loopCount > 0) {
		// we're not repeating and we've finished the envelope
		return baseFrequency + this->_totalAdjustment;
	}

	// otherwise we need to calculate the frequency
	int frequency = baseFrequency;

	// TODO cumulative adjustment (using loopCount * _totalAdjustment)

	for (auto phase : *this->_phases) {
		if (currentStep < phase.number) {
			frequency += (currentStep * phase.adjustment);
			break;
		}

		currentStep -= phase.number;
	}

	// TODO loop frequency value as appropriate

	return frequency;
}

bool SteppedFrequencyEnvelope::isFinished(uint16_t elapsed, long duration) {
	if (_repeats) {
		// a repeating frequency envelope never finishes
		return false;
	}

	return elapsed >= _totalLength;
}

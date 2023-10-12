#ifndef ENHANCED_SAMPLES_GENERATOR_H
#define ENHANCED_SAMPLES_GENERATOR_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <fabgl.h>

#include "audio_sample.h"
#include "types.h"

// Enhanced samples generator
//
class EnhancedSamplesGenerator : public WaveformGenerator {
	public:
		EnhancedSamplesGenerator(std::shared_ptr<audio_sample> sample);

		void setFrequency(int value);
		int getSample();
	
	private:
		std::weak_ptr<audio_sample> _sample;
		int _index;
		int _blockIndex;
};

EnhancedSamplesGenerator::EnhancedSamplesGenerator(std::shared_ptr<audio_sample> sample) 
	: _sample(sample), _index(0), _blockIndex(0)
{}

void EnhancedSamplesGenerator::setFrequency(int value) {
	// usually this will do nothing...
	// but we'll hijack this method to allow us to reset the sample index
	// ideally we'd override the enable method, but C++ doesn't let us do that
	if (value < 0) {
		_index = 0;
		_blockIndex = 0;
	}
}

int EnhancedSamplesGenerator::getSample() {
	if (duration() == 0 || _sample.expired()) {
		return 0;
	}

	auto samplePtr = _sample.lock();
	auto block = samplePtr->blocks[_blockIndex];
	int8_t sample = block->getBuffer()[_index++];
	
	// Insert looping magic here
	if (_index >= block->size()) {
		// block reached end, move to next, or loop
		_index = 0;
		_blockIndex++;
		if (_blockIndex >= samplePtr->blocks.size()) {
			_blockIndex = 0;
		}
	}
	if (samplePtr->format == AUDIO_FORMAT_8BIT_UNSIGNED) {
		sample = sample - 128;
	}

	// process volume
	sample = sample * volume() / 127;

	decDuration();

	return sample;
}

#endif // ENHANCED_SAMPLES_GENERATOR_H

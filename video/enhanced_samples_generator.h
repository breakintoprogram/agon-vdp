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
		EnhancedSamplesGenerator(std::shared_ptr<AudioSample> sample);

		void setFrequency(int value);
		int getSample();

		int getDuration();

	private:
		std::weak_ptr<AudioSample> _sample;

		int previousSample = 0;
		int currentSample = 0;
		double samplesPerGet = 1.0;
		double fractionalSampleOffset = 0.0;
};

EnhancedSamplesGenerator::EnhancedSamplesGenerator(std::shared_ptr<AudioSample> sample)
	: _sample(sample)
{}

void EnhancedSamplesGenerator::setFrequency(int value) {
	// We'll hijack this method to allow us to reset the sample index
	// ideally we'd override the enable method, but C++ doesn't let us do that
	if (!_sample.expired()) {
		auto samplePtr = _sample.lock();
		if (value < 0) {
			// rewind our sample if it's still valid
			samplePtr->rewind();

			fractionalSampleOffset = 0.0;
			previousSample = samplePtr->getSample();
			currentSample = samplePtr->getSample();
		} else {
			samplesPerGet = (double)value / (double)(samplePtr->sampleRate);
		}
	}
}

int EnhancedSamplesGenerator::getSample() {
	if (duration() == 0 || _sample.expired()) {
		return 0;
	}

	auto samplePtr = _sample.lock();

	while (fractionalSampleOffset >= 1.0) {
		previousSample = currentSample;
		currentSample = samplePtr->getSample();
		fractionalSampleOffset -= 1.0;
	}

	 // Interpolate between the samples to reduce aliasing
	int sample = currentSample * fractionalSampleOffset + previousSample * (1.0-fractionalSampleOffset);

	fractionalSampleOffset += samplesPerGet;

	// process volume
	sample = sample * volume() / 127;

	decDuration();

	return sample;
}

int EnhancedSamplesGenerator::getDuration() {
	// NB this is hard-coded for a 16khz sample rate
	return _sample.expired() ? 0 : _sample.lock()->getDuration() / samplesPerGet;
}

#endif // ENHANCED_SAMPLES_GENERATOR_H

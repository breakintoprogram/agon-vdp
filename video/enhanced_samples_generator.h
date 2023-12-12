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
		void setSampleRate(int value);
		int getSample();

		int getDuration(uint16_t frequency);

		void seekTo(uint32_t position);
	private:
		std::weak_ptr<AudioSample> _sample;

		uint32_t		index;				// Current index inside the current sample block
		uint32_t		blockIndex;			// Current index into the sample data blocks
		int32_t			repeatCount = 0;	// Sample count when repeating
		// TODO consider whether repeatStart and repeatLength may need to be here
		// which would allow for per-channel repeat settings

		int				frequency = 0;
		int				previousSample = 0;
		int				currentSample = 0;
		double			samplesPerGet = 1.0;
		double			fractionalSampleOffset = 0.0;

		double calculateSamplerate(uint16_t frequency);
		int8_t getNextSample();
};

EnhancedSamplesGenerator::EnhancedSamplesGenerator(std::shared_ptr<AudioSample> sample)
	: _sample(sample)
{}

void EnhancedSamplesGenerator::setFrequency(int value) {
	frequency = value;
	samplesPerGet = calculateSamplerate(value);
}

void EnhancedSamplesGenerator::setSampleRate(int value) {
	WaveformGenerator::setSampleRate(value);
	samplesPerGet = calculateSamplerate(frequency);
}

int EnhancedSamplesGenerator::getSample() {
	if (duration() == 0 || _sample.expired()) {
		return 0;
	}

	auto samplePtr = _sample.lock();

	// if we've moved far enough along, read the next sample
	while (fractionalSampleOffset >= 1.0) {
		previousSample = currentSample;
		currentSample = getNextSample();
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

int EnhancedSamplesGenerator::getDuration(uint16_t frequency) {
	return _sample.expired() ? 0 : _sample.lock()->getDuration() / calculateSamplerate(frequency);
}

void EnhancedSamplesGenerator::seekTo(uint32_t position) {
	if (!_sample.expired()) {
		auto samplePtr = _sample.lock();
		samplePtr->seekTo(position, index, blockIndex, repeatCount);

		// prepare our fractional sample data for playback
		fractionalSampleOffset = 0.0;
		previousSample = samplePtr->getSample(index, blockIndex);
		currentSample = samplePtr->getSample(index, blockIndex);
	}
}

double EnhancedSamplesGenerator::calculateSamplerate(uint16_t frequency) {
	if (!_sample.expired()) {
		auto samplePtr = _sample.lock();
		auto baseFrequency = samplePtr->baseFrequency;
		auto frequencyAdjust = baseFrequency > 0 ? (double)frequency / (double)baseFrequency : 1.0;
		return frequencyAdjust * ((double)samplePtr->sampleRate / (double)(sampleRate()));
	}
	return 1.0;
}

int8_t EnhancedSamplesGenerator::getNextSample() {
	if (!_sample.expired()) {
		auto samplePtr = _sample.lock();
		auto sample = samplePtr->getSample(index, blockIndex);
		
		// looping magic
		repeatCount--;
		if (repeatCount == 0) {
			// we've reached the end of the repeat section, so loop back
			seekTo(samplePtr->repeatStart);
		}

		return sample;
	}
	return 0;
}

#endif // ENHANCED_SAMPLES_GENERATOR_H

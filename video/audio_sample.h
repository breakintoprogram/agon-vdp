#ifndef AUDIO_SAMPLE_H
#define AUDIO_SAMPLE_H

#include <memory>
#include <unordered_map>

#include "types.h"
#include "audio_channel.h"
#include "buffer_stream.h"

struct AudioSample {
	AudioSample(std::vector<std::shared_ptr<BufferStream>>& streams, uint8_t format, uint32_t sampleRate = AUDIO_DEFAULT_SAMPLE_RATE, uint16_t frequency = 0) :
		blocks(streams), format(format), sampleRate(sampleRate), baseFrequency(frequency) {}
	~AudioSample();

	int8_t getSample(uint32_t & index, uint32_t & blockIndex);
	void seekTo(uint32_t position, uint32_t & index, uint32_t & blockIndex, int32_t & repeatCount);
	uint32_t getSize();
	uint32_t getDuration();

	std::vector<std::shared_ptr<BufferStream>>& blocks;
	uint8_t			format;				// Format of the sample data
	uint32_t		sampleRate;			// Sample rate of the sample
	uint16_t		baseFrequency = 0;	// Base frequency of the sample
	int32_t			repeatStart = 0;	// Start offset for repeat, in samples
	int32_t			repeatLength = -1;	// Length of the repeat section in samples, -1 means to end of sample
	std::unordered_map<uint8_t, std::weak_ptr<AudioChannel>> channels;	// Channels playing this sample
};

AudioSample::~AudioSample() {
	// iterate over channels
	for (auto channelPair : this->channels) {
		auto channelRef = channelPair.second;
		if (!channelRef.expired()) {
			auto channel = channelRef.lock();
			debug_log("AudioSample: removing sample from channel %d\n\r", channel->channel());
			// Remove sample from channel
			channel->setWaveform(AUDIO_WAVE_DEFAULT, nullptr);
		}
	}
	debug_log("AudioSample cleared\n\r");
}

int8_t AudioSample::getSample(uint32_t & index, uint32_t & blockIndex) {
	// get the next sample
	if (blockIndex >= blocks.size()) {
		// we've reached the end of the sample, and haven't looped, so return 0 (silence)
		return 0;
	}

	auto block = blocks[blockIndex];
	int8_t sample = block->getBuffer()[index++];

	if (index >= block->size()) {
		// block reached end, move to next block
		index = 0;
		blockIndex++;
	}

	if (format == AUDIO_FORMAT_8BIT_UNSIGNED) {
		sample = sample - 128;
	}

	return sample;
}

void AudioSample::seekTo(uint32_t position, uint32_t & index, uint32_t & blockIndex, int32_t & repeatCount) {
	// NB repeatCount calculation here can result in zero, or a negative number,
	// or a number that's beyond the end of the sample, which is fine
	// it just means that the sample will never loop
	if (repeatLength < 0) {
		// repeat to end of sample
		repeatCount = getSize() - position;
	} else if (repeatLength > 0) {
		auto repeatEnd = repeatStart + repeatLength;
		repeatCount = repeatEnd - position;
	} else {
		repeatCount = 0;
	}

	blockIndex = 0;
	index = position;
	while (blockIndex < blocks.size() && index >= blocks[blockIndex]->size()) {
		index -= blocks[blockIndex]->size();
		blockIndex++;
	}
}

uint32_t AudioSample::getSize() {
	uint32_t samples = 0;
	for (auto block : blocks) {
		samples += block->size();
	}
	return samples;
}

uint32_t AudioSample::getDuration() {
	// returns duration of sample in ms when played back without tuning
	return (getSize() * 1000) / sampleRate;
}

#endif // AUDIO_SAMPLE_H

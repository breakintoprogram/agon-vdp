#ifndef AUDIO_SAMPLE_H
#define AUDIO_SAMPLE_H

#include <memory>
#include <unordered_map>

#include "types.h"
#include "audio_channel.h"
#include "buffer_stream.h"

struct audio_sample {
	audio_sample(std::vector<std::shared_ptr<BufferStream>> streams, uint8_t format) : blocks(streams), format(format) {}
	~audio_sample();
	std::vector<std::shared_ptr<BufferStream>> blocks;
	uint8_t			format = 0;			// Format of the sample data
	std::unordered_map<uint8_t, std::weak_ptr<audio_channel>> channels;	// Channels playing this sample
};

audio_sample::~audio_sample() {
	// iterate over channels
	for (auto channelPair : this->channels) {
		auto channelRef = channelPair.second;
		if (!channelRef.expired()) {
			auto channel = channelRef.lock();
			debug_log("audio_sample: removing sample from channel %d\n\r", channel->channel());
			// Remove sample from channel
			channel->setWaveform(AUDIO_WAVE_DEFAULT, nullptr);
		}
	}
	debug_log("audio_sample cleared\n\r");
}

#endif // AUDIO_SAMPLE_H

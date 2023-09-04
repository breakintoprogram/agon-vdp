#ifndef VDU_BUFFERED_H
#define VDU_BUFFERED_H

#include <memory>
#include <vector>
#include <unordered_map>

#include "agon.h"
#include "buffer_stream.h"
#include "multi_buffer_stream.h"
#include "types.h"

std::unordered_map<uint8_t, std::vector<std::shared_ptr<BufferStream>, psram_allocator<std::shared_ptr<BufferStream>>>> buffers;

// VDU 23, 0, &A0, bufferId, command: Buffered command support
//
void VDUStreamProcessor::vdu_sys_buffered() {
	auto bufferId = readByte_t();
	auto command = readByte_t();

	switch (command) {
		case BUFFERED_WRITE: {
			bufferWrite(bufferId);
		}	break;
		case BUFFERED_CALL: {
			bufferCall(bufferId);
		}	break;
		case BUFFERED_CLEAR: {
			bufferClear(bufferId);
		}	break;
		case BUFFERED_ADJUST: {
			bufferAdjust(bufferId);
		}	break;
	}
}

// VDU 23, 0, &A0, bufferId, 0, length; data...: store stream into buffer
// This adds a new stream to the given bufferId
// allowing a single bufferId to store multiple streams of data
//
void VDUStreamProcessor::bufferWrite(uint8_t bufferId) {
	auto length = readWord_t();
	auto bufferStream = make_shared_psram<BufferStream>(length);

	debug_log("bufferWrite: storing stream into buffer %d, length %d\n\r", bufferId, length);

	for (auto i = 0; i < length; i++) {
		auto data = readByte_b();
		bufferStream->writeBufferByte(data, i);
	}

	buffers[bufferId].push_back(std::move(bufferStream));
	debug_log("bufferWrite: stored stream in buffer %d, length %d, %d streams stored\n\r", bufferId, length, buffers[bufferId].size());
}

// VDU 23, 0, &A0, bufferId, 1: Call buffer
// Processes all commands from the streams stored against the given bufferId
//
void VDUStreamProcessor::bufferCall(uint8_t bufferId) {
	debug_log("bufferCall: buffer %d\n\r", bufferId);
	if (buffers.find(bufferId) != buffers.end()) {
		auto streams = buffers[bufferId];
		auto multiBufferStream = make_shared_psram<MultiBufferStream>(streams);
		auto streamProcessor = make_unique_psram<VDUStreamProcessor>((Stream *)multiBufferStream.get());
		streamProcessor->processAllAvailable();
	} else {
		debug_log("bufferCall: buffer %d not found\n\r", bufferId);
	}
}

// VDU 23, 0, &A0, bufferId, 2: Clear buffer
// Removes all streams stored against the given bufferId
//
void VDUStreamProcessor::bufferClear(uint8_t bufferId) {
	debug_log("bufferClear: buffer %d\n\r", bufferId);
	if (buffers.find(bufferId) != buffers.end()) {
		buffers.erase(bufferId);
	} else {
		debug_log("bufferClear: buffer %d not found\n\r", bufferId);
	}
}

// VDU 23, 0, &A0, bufferId, 3: Adjust buffer
// TODO...
//
void VDUStreamProcessor::bufferAdjust(uint8_t bufferId) {
	// TODO implement commands to adjust buffer contents
	// ideas include:
	// overwrite byte
	// overwrite multiple bytes
	// insert byte(s)
	// increment by amount
	// increment with carry
	// other maths/logic operations on bytes within buffer
	// copy bytes from one buffer to another

	// then there is things like conditional call
	//   based on a byte value within a given buffer
	//   consider different comparisons (==, !=, <, >, <=, >=)
	//   and comparisons with words, longs, etc
	// possibly jump to a given or relative buffer position
	//   altho it may be more sensible to just encourage calling a different buffer

	// there _may_ be value in increasing the size of our bufferId to 16bits
}

#endif // VDU_BUFFERED_H

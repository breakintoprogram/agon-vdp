#ifndef BUFFER_STREAM_H
#define BUFFER_STREAM_H

#include <memory>
#include <Stream.h>

#include "types.h"

class BufferStream : Stream {
	public:
		BufferStream(uint32_t bufferLength);
		int available();
		int read();
		int peek();
		size_t write(uint8_t b);

		void rewind();

		inline uint8_t * getBuffer() {
			return buffer.get();
		}
		inline uint32_t size() {
			return bufferLength;
		}
		bool writeBuffer(uint8_t * data, uint32_t length, uint32_t offset);
		void writeBufferByte(uint8_t data, uint32_t offset);
		bool incrementBufferByte(uint32_t offset, int8_t by);
	private:
		std::unique_ptr<uint8_t[]> buffer;
		uint32_t bufferLength;
		uint32_t bufferPosition;
};

BufferStream::BufferStream(uint32_t bufferLength) : bufferLength(bufferLength) {
	buffer = make_unique_psram_array<uint8_t>(bufferLength);
	rewind();
}

int BufferStream::available() {
	return bufferLength - bufferPosition;
}

int BufferStream::read() {
	if (bufferPosition < bufferLength) {
		return buffer[bufferPosition++];
	}
	return -1;
}

int BufferStream::peek() {
	if (bufferPosition < bufferLength) {
		return buffer[bufferPosition];
	}
	return -1;
}

void BufferStream::rewind() {
	bufferPosition = 0;
}

size_t BufferStream::write(uint8_t b) {
	// write is not supported
	return 0;
}

bool BufferStream::writeBuffer(uint8_t * data, uint32_t length, uint32_t offset = 0) {
	// TODO consider return type - we could support writing to buffer limit,
	// and returning how many bytes were written
	if (length + offset <= bufferLength) {
		memcpy(buffer.get() + offset, data, length);
		return true;
	} else {
		debug_log("BufferStream::writeBuffer: buffer overflow\n\r");
		return false;
	}
}

void BufferStream::writeBufferByte(uint8_t data, uint32_t offset = 0) {
	if (offset < bufferLength) {
		buffer[offset] = data;
	}
}

// incrementBufferByte
// accepts an offset and a value to increment by
// returns true if value overflowed
bool BufferStream::incrementBufferByte(uint32_t offset = 0, int8_t by = 1) {
	if (offset < bufferLength) {
		auto oldValue = buffer[offset];
		buffer[offset] += by;

		// check for overflow
		if (by > 0) {
			return buffer[offset] < oldValue;
		} else {
			return buffer[offset] > oldValue;
		}
	}
	return false;
}

#endif // BUFFER_STREAM_H

#ifndef VDU_BUFFERED_H
#define VDU_BUFFERED_H

#include <memory>
#include <vector>
#include <unordered_map>

#include "agon.h"
#include "buffer_stream.h"
#include "multi_buffer_stream.h"
#include "types.h"

std::unordered_map<uint16_t, std::vector<std::shared_ptr<BufferStream>, psram_allocator<std::shared_ptr<BufferStream>>>> buffers;

// VDU 23, 0, &A0, bufferId; command: Buffered command support
//
void VDUStreamProcessor::vdu_sys_buffered() {
	auto bufferId = readWord_t();
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
		case BUFFERED_CREATE: {
			bufferCreate(bufferId);
		}	break;
		case BUFFERED_SET_OUTPUT: {
			setOutputStream(bufferId);
		}	break;
		case BUFFERED_ADJUST: {
			bufferAdjust(bufferId);
		}	break;
		case BUFFERED_DEBUG_INFO: {
			debug_log("vdu_sys_buffered: buffer %d, %d streams stored\n\r", bufferId, buffers[bufferId].size());
			// output contents of buffer stream 0
			auto buffer = buffers[bufferId][0];
			auto bufferLength = buffer->size();
			for (auto i = 0; i < bufferLength; i++) {
				auto data = buffer->getBuffer()[i];
				debug_log("%02X ", data);
			}
			debug_log("\n\r");
		}	break;
		default: {
			debug_log("vdu_sys_buffered: unknown command %d, buffer %d\n\r", command, bufferId);
		}	break;
	}
}

// VDU 23, 0, &A0, bufferId; 0, length; data...: store stream into buffer
// This adds a new stream to the given bufferId
// allowing a single bufferId to store multiple streams of data
//
void VDUStreamProcessor::bufferWrite(uint16_t bufferId) {
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

// VDU 23, 0, &A0, bufferId; 1: Call buffer
// Processes all commands from the streams stored against the given bufferId
//
void VDUStreamProcessor::bufferCall(uint16_t bufferId) {
	debug_log("bufferCall: buffer %d\n\r", bufferId);
	if (buffers.find(bufferId) != buffers.end()) {
		auto streams = buffers[bufferId];
		auto multiBufferStream = make_shared_psram<MultiBufferStream>(streams);
		// TODO consider - should this use originalOutputStream?
		auto streamProcessor = make_unique_psram<VDUStreamProcessor>(multiBufferStream, outputStream);
		streamProcessor->processAllAvailable();
	} else {
		debug_log("bufferCall: buffer %d not found\n\r", bufferId);
	}
}

// VDU 23, 0, &A0, bufferId; 2: Clear buffer
// Removes all streams stored against the given bufferId
// sending a bufferId of 65535 (i.e. -1) clears all buffers
//
void VDUStreamProcessor::bufferClear(uint16_t bufferId) {
	debug_log("bufferClear: buffer %d\n\r", bufferId);
	if (bufferId == 65535) {
		buffers.clear();
		return;
	}
	if (buffers.find(bufferId) != buffers.end()) {
		buffers.erase(bufferId);
		debug_log("bufferClear: cleared buffer %d\n\r", bufferId);
	} else {
		debug_log("bufferClear: buffer %d not found\n\r", bufferId);
	}
}

// VDU 23, 0, &A0, bufferId; 3, size; : Create a writeable buffer
// This is used for creating buffers to redirect output to
//
void VDUStreamProcessor::bufferCreate(uint16_t bufferId) {
	auto size = readWord_t();
	if (size == -1) {
		// timeout
		return;
	}
	if (bufferId == 0) {
		debug_log("bufferCreate: bufferId 0 is reserved\n\r");
		return;
	}
	if (buffers.find(bufferId) != buffers.end()) {
		debug_log("bufferCreate: buffer %d already exists\n\r", bufferId);
		return;
	}
	auto buffer = make_shared_psram<WritableBufferStream>(size);
	// Ensure buffer is empty
	for (auto i = 0; i < size; i++) {
		buffer->writeBufferByte(0, i);
	}
	buffers[bufferId].push_back(buffer);
	debug_log("bufferCreate: created buffer %d, size %d\n\r", bufferId, size);
}

// VDU 23, 0, &A0, bufferId; 4: Set output to buffer
// use an ID of -1 (65535) to clear the output buffer (no output)
// use an ID of 0 to reset the output buffer to it's original value
//
void VDUStreamProcessor::setOutputStream(uint16_t bufferId) {
	if (bufferId == 65535) {
		outputStream = nullptr;
		return;
	}
	// bufferId of 0 resets output buffer to it's original value
	// which will usually be the z80 serial port
	if (bufferId == 0) {
		outputStream = originalOutputStream;
		return;
	}
	if (buffers.find(bufferId) != buffers.end()) {
		auto output = buffers[bufferId][0];
		if (output->isWritable()) {
			outputStream = output;
		} else {
			debug_log("setOutputStream: buffer %d is not writable\n\r", bufferId);
		}
	} else {
		debug_log("setOutputStream: buffer %d not found\n\r", bufferId);
	}
}

uint16_t VDUStreamProcessor::getBufferByte(uint16_t bufferId, uint32_t offset) {
	if (buffers.find(bufferId) != buffers.end()) {
		// buffer ID exists
		// loop thru buffers stored against this ID to find data at offset
		uint32_t value = 0;
		auto currentOffset = offset;
		for (auto buffer : buffers[bufferId]) {
			auto bufferLength = buffer->size();
			if (currentOffset < bufferLength) {
				return buffer->getBuffer()[currentOffset];
			}
			currentOffset -= bufferLength;
		}
	}
	return -1;
}

bool VDUStreamProcessor::setBufferByte(uint8_t value, uint16_t bufferId, uint32_t offset) {
	if (buffers.find(bufferId) != buffers.end()) {
		// buffer ID exists
		// find the buffer containing the offset
		auto currentOffset = offset;
		for (auto buffer : buffers[bufferId]) {
			auto bufferLength = buffer->size();
			if (currentOffset < bufferLength) {
				buffer->writeBufferByte(value, currentOffset);
				return true;
			}
			currentOffset -= bufferLength;
		}
	}
	return false;
}

// VDU 23, 0, &A0, bufferId; 5: Adjust buffer
// Format of command:
// VDU 23, 0, &A0, bufferId; 5, command, offset; [count;] [operand]
// operand will vary depending on higher bits set in command
// if it's a buffer-originated value it will be a buffer ID and offset
// otherwise it will be an immediate value
// count will only be present if MULTI_SRC or MULTI_DST bit is set
// NB command only supports 16-bit offset values (for now)
// we could consider supporting 24-bit offsets in future,
// either by using another bit in command, or re-purposing the ADJUST_16 bit
void VDUStreamProcessor::bufferAdjust(uint16_t bufferId) {
	auto command = readByte_t();
	auto offset = readWord_t();

	// bool use16Bit = command & ADJUST_16;
	bool useBufferValue = command & ADJUST_BUFFER_VALUE;
	bool useMultiDestination = command & ADJUST_MULTI_DST;
	bool useMultiSource = command & ADJUST_MULTI_SRC;
	uint8_t op = command & ADJUST_OP_MASK;
	// Operators that are NOT or greater do not have an operand value
	bool hasOperand = op < ADJUST_NOT;

	auto operandBufferId = -1;
	auto operandOffset = 0;
	auto count = 1;

	if (useMultiDestination | useMultiSource) {
		count = readWord_t();
	}
	if (useBufferValue && hasOperand) {
		operandBufferId = readWord_t();
		operandOffset = readWord_t();
	}

	// destination = destination <operator> [operand]
	//
	// All operators are 1 byte (command)
	// Not all operators require an operand - NOT and SET do not

	switch (op) {
		case ADJUST_ADD: {
			auto sourceValue = getBufferByte(bufferId, offset);
			auto operandValue = useBufferValue ? getBufferByte(operandBufferId, operandOffset) : readByte_t();
			if (sourceValue == -1 || operandValue == -1) {
				debug_log("bufferAdjust: invalid source or operand value\n\r");
				return;
			}
			auto result = sourceValue + operandValue;
			if (setBufferByte(result, bufferId, offset)) {
				debug_log("bufferAdjust: add %d to %d at offset %d\n\r", operandValue, sourceValue, offset);
			} else {
				debug_log("bufferAdjust: failed to set result %d at offset %d\n\r", result, offset);
			}
		}	break;
		case ADJUST_ADD_CARRY: {
			// iterate thru count
			// store carry value in next byte

		}	break;
		case ADJUST_AND: {

		}	break;
		case ADJUST_OR: {

		}	break;
		case ADJUST_XOR: {

		}	break;
		case ADJUST_SET: {
			auto operandValue = useBufferValue ? getBufferByte(operandBufferId, operandOffset) : readByte_t();
			if (operandValue == -1) {
				debug_log("bufferAdjust: set value invalid operand value\n\r");
				return;
			}
			if (!setBufferByte(operandValue, bufferId, offset)) {
				debug_log("bufferAdjust: failed to set value %d in buffer %d at offset %d\n\r", operandValue, bufferId, offset);
			}
		}	break;
		case ADJUST_NOT: {
			auto sourceValue = getBufferByte(bufferId, offset);
			if (sourceValue == -1) {
				debug_log("bufferAdjust: invalid source value\n\r");
				return;
			}
			uint8_t result = ~(uint8_t)sourceValue;
			debug_log("bufferAdjust: NOT %X at offset %d = %X\n\r", sourceValue, offset, result);
			setBufferByte(result, bufferId, offset);
		}	break;
	}

	// TODO implement commands to adjust buffer contents
	// ideas include:
	// overwrite byte
	// overwrite multiple bytes
	// insert byte(s)
	// increment by amount
	// increment with carry
	// other maths/logic operations on bytes within buffer
	// copy bytes from one buffer to another

	// change buffer write offset

	// then there is things like conditional call
	//   based on a byte value within a given buffer
	//   consider different comparisons (==, !=, <, >, <=, >=)
	//   and comparisons with words, longs, etc
	// possibly jump to a given or relative buffer position
	//   altho it may be more sensible to just encourage calling a different buffer
}

#endif // VDU_BUFFERED_H

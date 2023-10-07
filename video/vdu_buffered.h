#ifndef VDU_BUFFERED_H
#define VDU_BUFFERED_H

#include <algorithm>
#include <memory>
#include <vector>
#include <unordered_map>

#include "agon.h"
#include "buffer_stream.h"
#include "multi_buffer_stream.h"
#include "sprites.h"
#include "types.h"

std::unordered_map<uint16_t, std::vector<std::shared_ptr<BufferStream>, psram_allocator<std::shared_ptr<BufferStream>>>> buffers;

// VDU 23, 0, &A0, bufferId; command: Buffered command support
//
void VDUStreamProcessor::vdu_sys_buffered() {
	auto bufferId = readWord_t();
	auto command = readByte_t();

	switch (command) {
		case BUFFERED_WRITE: {
			auto length = readWord_t(); if (length == -1) return;
			bufferWrite(bufferId, length);
		}	break;
		case BUFFERED_CALL: {
			bufferCall(bufferId, 0);
		}	break;
		case BUFFERED_CLEAR: {
			bufferClear(bufferId);
		}	break;
		case BUFFERED_CREATE: {
			auto size = readWord_t(); if (size == -1) return;
			bufferCreate(bufferId, size);
		}	break;
		case BUFFERED_SET_OUTPUT: {
			setOutputStream(bufferId);
		}	break;
		case BUFFERED_ADJUST: {
			bufferAdjust(bufferId);
		}	break;
		case BUFFERED_COND_CALL: {
			// VDU 23, 0, &A0, bufferId; 6, <conditional arguments>  : Conditional call
			if (bufferConditional()) {
				bufferCall(bufferId, 0);
			}
		}	break;
		case BUFFERED_JUMP: {
			bufferJump(bufferId, 0);
		}	break;
		case BUFFERED_COND_JUMP: {
			// VDU 23, 0, &A0, bufferId; 8, <conditional arguments>  : Conditional jump
			if (bufferConditional()) {
				bufferJump(bufferId, 0);
			}
		}	break;
		case BUFFERED_OFFSET_JUMP: {
			// VDU 23, 0, &A0, bufferId; 9, offset; offsetHighByte  : Offset jump
			auto offset = getOffsetFromStream(bufferId, true); if (offset == -1) return;
			bufferJump(bufferId, offset);
		}	break;
		case BUFFERED_OFFSET_COND_JUMP: {
			// VDU 23, 0, &A0, bufferId; &0A, offset; offsetHighByte, <conditional arguments>  : Conditional jump with offset
			auto offset = getOffsetFromStream(bufferId, true); if (offset == -1) return;
			if (bufferConditional()) {
				bufferJump(bufferId, offset);
			}
		}	break;
		case BUFFERED_OFFSET_CALL: {
			// VDU 23, 0, &A0, bufferId; &0B, offset; offsetHighByte  : Offset call
			auto offset = getOffsetFromStream(bufferId, true); if (offset == -1) return;
			bufferCall(bufferId, offset);
		}	break;
		case BUFFERED_OFFSET_COND_CALL: {
			// VDU 23, 0, &A0, bufferId; &0C, offset; offsetHighByte, <conditional arguments>  : Conditional call with offset
			auto offset = getOffsetFromStream(bufferId, true); if (offset == -1) return;
			if (bufferConditional()) {
				bufferCall(bufferId, offset);
			}
		}	break;
		case BUFFERED_COPY: {
			bufferCopy(bufferId);
		}	break;
		case BUFFERED_CONSOLIDATE: {
			bufferConsolidate(bufferId);
		}	break;
		case BUFFERED_SPLIT: {
			auto length = readWord_t(); if (length == -1) return;
			bufferSplit(bufferId, length);
		}	break;
		case BUFFERED_REVERSE_BLOCKS: {
			bufferReverseBlocks(bufferId);
		}	break;
		case BUFFERED_REVERSE: {
			auto options = readByte_t(); if (options == -1) return;
			bufferReverse(bufferId, options);
		}	break;
		case BUFFERED_DEBUG_INFO: {
			debug_log("vdu_sys_buffered: buffer %d, %d streams stored\n\r", bufferId, buffers[bufferId].size());
			if (buffers[bufferId].size() == 0) {
				return;
			}
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
uint32_t VDUStreamProcessor::bufferWrite(uint16_t bufferId, uint32_t length) {
	auto bufferStream = make_shared_psram<BufferStream>(length);

	debug_log("bufferWrite: storing stream into buffer %d, length %d\n\r", bufferId, length);

	auto remaining = readIntoBuffer(bufferStream->getBuffer(), length);
	if (remaining > 0) {
		// NB this discards the data we just read
		debug_log("bufferWrite: timed out write for buffer %d (%d bytes remaining)\n\r", bufferId, remaining);
		return remaining;
	}

	buffers[bufferId].push_back(std::move(bufferStream));
	debug_log("bufferWrite: stored stream in buffer %d, length %d, %d streams stored\n\r", bufferId, length, buffers[bufferId].size());
	return remaining;
}

// VDU 23, 0, &A0, bufferId; 1: Call buffer
// Processes all commands from the streams stored against the given bufferId
//
void VDUStreamProcessor::bufferCall(uint16_t bufferId, uint32_t offset) {
	debug_log("bufferCall: buffer %d\n\r", bufferId);
	if (bufferId == 65535) {
		// buffer ID of -1 (65535) is used as a "null output" stream
		return;
	}
	if (buffers.find(bufferId) != buffers.end()) {
		if (id == 65535 && inputStream->available() == 0) {
			// tail-call optimise - turn the call into a jump
			bufferJump(bufferId, offset);
			return;
		}
		auto streams = buffers[bufferId];
		auto multiBufferStream = make_shared_psram<MultiBufferStream>(streams);
		if (offset) {
			multiBufferStream->seekTo(offset);
		}
		debug_log("bufferCall: processing %d streams\n\r", streams.size());
		auto streamProcessor = make_unique_psram<VDUStreamProcessor>(multiBufferStream, outputStream, bufferId);
		streamProcessor->processAllAvailable();
		return;
	}
	debug_log("bufferCall: buffer %d not found\n\r", bufferId);
}

// VDU 23, 0, &A0, bufferId; 2: Clear buffer
// Removes all streams stored against the given bufferId
// sending a bufferId of 65535 (i.e. -1) clears all buffers
//
void VDUStreamProcessor::bufferClear(uint16_t bufferId) {
	debug_log("bufferClear: buffer %d\n\r", bufferId);
	if (bufferId == 65535) {
		buffers.clear();
		resetBitmaps();
		return;
	}
	if (buffers.find(bufferId) != buffers.end()) {
		buffers.erase(bufferId);
		clearBitmap(bufferId);
		debug_log("bufferClear: cleared buffer %d\n\r", bufferId);
	} else {
		debug_log("bufferClear: buffer %d not found\n\r", bufferId);
	}
}

// VDU 23, 0, &A0, bufferId; 3, size; : Create a writeable buffer
// This is used for creating buffers to redirect output to
//
std::shared_ptr<WritableBufferStream> VDUStreamProcessor::bufferCreate(uint16_t bufferId, uint32_t size) {
	if (bufferId == 0 || bufferId == 65535) {
		debug_log("bufferCreate: bufferId %d is reserved\n\r", bufferId);
		return nullptr;
	}
	if (buffers.find(bufferId) != buffers.end()) {
		debug_log("bufferCreate: buffer %d already exists\n\r", bufferId);
		return nullptr;
	}
	auto buffer = make_shared_psram<WritableBufferStream>(size);
	if (!buffer) {
		debug_log("bufferCreate: failed to create buffer %d\n\r", bufferId);
		return nullptr;
	}
	// Ensure buffer is empty
	for (auto i = 0; i < size; i++) {
		buffer->writeBufferByte(0, i);
	}
	buffers[bufferId].push_back(buffer);
	debug_log("bufferCreate: created buffer %d, size %d\n\r", bufferId, size);
	return buffer;
}

// VDU 23, 0, &A0, bufferId; 4: Set output to buffer
// use an ID of -1 (65535) to clear the output buffer (no output)
// use an ID of 0 to reset the output buffer to it's original value
//
// TODO add a variant/command to adjust offset inside output stream
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

uint32_t VDUStreamProcessor::getOffsetFromStream(uint16_t bufferId, bool isAdvanced) {
	if (isAdvanced) {
		auto offset = read24_t();
		if (offset == -1) {
			return -1;
		}
		if (offset & 0x00800000) {
			// top bit of 24-bit offset is set, so we have a block offset too
			auto blockOffset = readWord_t();
			if (blockOffset == -1) {
				return -1;
			}
			if (buffers.find(bufferId) == buffers.end()) {
				debug_log("getOffsetFromStream: buffer %d not found\n\r", bufferId);
				return -1;
			}
			if (blockOffset >= buffers[bufferId].size()) {
				debug_log("getOffsetFromStream: block offset %d is greater than number of blocks %d\n\r", blockOffset, buffers[bufferId].size());
				return -1;
			}
			// calculate our true offset
			// by counting up sizes of all blocks before the one we want
			auto trueOffset = offset & 0x007FFFFF;
			for (auto i = 0; i < blockOffset; i++) {
				trueOffset += buffers[bufferId][i]->size();
			}
			return trueOffset;
		}
		return offset;
	}
	return readWord_t();
}

int16_t VDUStreamProcessor::getBufferByte(uint16_t bufferId, uint32_t offset) {
	if (buffers.find(bufferId) != buffers.end()) {
		// buffer ID exists
		// loop thru buffers stored against this ID to find data at offset
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

// VDU 23, 0, &A0, bufferId; 5, operation, offset; [count;] [operand]: Adjust buffer
// This is used for adjusting the contents of a buffer
// It can be used to overwrite bytes, insert bytes, increment bytes, etc
// Basic operation are not, neg, set, add, add-with-carry, and, or, xor
// Upper bits of operation byte are used to indicate:
// - whether to use a long offset (24-bit) or short offset (16-bit)
// - whether the operand is a buffer-originated value or an immediate value
// - whether to adjust a single target or multiple targets
// - whether to use a single operand or multiple operands
//
void VDUStreamProcessor::bufferAdjust(uint16_t bufferId) {
	auto command = readByte_t();

	bool useAdvancedOffsets = command & ADJUST_ADVANCED_OFFSETS;
	bool useBufferValue = command & ADJUST_BUFFER_VALUE;
	bool useMultiTarget = command & ADJUST_MULTI_TARGET;
	bool useMultiOperand = command & ADJUST_MULTI_OPERAND;
	uint8_t op = command & ADJUST_OP_MASK;
	// Operators that are greater than NEG have an operand value
	bool hasOperand = op > ADJUST_NEG;

	auto offset = getOffsetFromStream(bufferId, useAdvancedOffsets);
	auto operandBufferId = 0;
	auto operandOffset = 0;
	auto count = 1;

	if (useMultiTarget | useMultiOperand) {
		count = useAdvancedOffsets ? read24_t() : readWord_t();
	}
	if (useBufferValue && hasOperand) {
		operandBufferId = readWord_t();
		operandOffset = getOffsetFromStream(operandBufferId, useAdvancedOffsets);
	}

	if (command == -1 || count == -1 || offset == -1 || operandBufferId == -1 || operandOffset == -1) {
		debug_log("bufferAdjust: invalid command, count, offset or operand value\n\r");
		return;
	}

	auto sourceValue = 0;
	auto operandValue = 0;
	auto carryValue = 0;
	bool usingCarry = false;

	// if useMultiTarget is set, the we're updating multiple source values
	// if useMultiOperand is also set, we get multiple operand values
	// so...
	// if both useMultiTarget and useMultiOperand are false we're updating a single source value with a single operand
	// if useMultiTarget is false and useMultiOperand is true we're adding all operand values to the same source value
	// if useMultiTarget is true and useMultiOperand is false we're adding the same operand to all source values
	// if both useMultiTarget and useMultiOperand are true we're adding each operand value to the corresponding source value

	if (!useMultiTarget) {
		// we have a singular source value
		sourceValue = getBufferByte(bufferId, offset);
	}
	if (hasOperand && !useMultiOperand) {
		// we have a singular operand value
		operandValue = useBufferValue ? getBufferByte(operandBufferId, operandOffset) : readByte_t();
	}

	debug_log("bufferAdjust: command %d, offset %d, count %d, operandBufferId %d, operandOffset %d, sourceValue %d, operandValue %d\n\r", command, offset, count, operandBufferId, operandOffset, sourceValue, operandValue);
	debug_log("useMultiTarget %d, useMultiOperand %d, useAdvancedOffsets %d, useBufferValue %d\n\r", useMultiTarget, useMultiOperand, useAdvancedOffsets, useBufferValue);

	for (auto i = 0; i < count; i++) {
		if (useMultiTarget) {
			// multiple source values will change
			sourceValue = getBufferByte(bufferId, offset + i);
		}
		if (hasOperand && useMultiOperand) {
			operandValue = useBufferValue ? getBufferByte(operandBufferId, operandOffset + i) : readByte_t();
		}
		if (sourceValue == -1 || operandValue == -1) {
			debug_log("bufferAdjust: invalid source or operand value\n\r");
			return;
		}

		switch (op) {
			case ADJUST_NOT: {
				sourceValue = ~sourceValue;
			}	break;
			case ADJUST_NEG: {
				sourceValue = -sourceValue;
			}	break;
			case ADJUST_SET: {
				sourceValue = operandValue;
			}	break;
			case ADJUST_ADD: {
				// byte-wise add - no carry, so bytes may overflow
				sourceValue = sourceValue + operandValue;
			}	break;
			case ADJUST_ADD_CARRY: {
				// byte-wise add with carry
				// bytes are treated as being in little-endian order
				usingCarry = true;
				sourceValue = sourceValue + operandValue + carryValue;
				if (sourceValue > 255) {
					carryValue = 1;
					sourceValue -= 256;
				} else {
					carryValue = 0;
				}
			}	break;
			case ADJUST_AND: {
				sourceValue = sourceValue & operandValue;
			}	break;
			case ADJUST_OR: {
				sourceValue = sourceValue | operandValue;
			}	break;
			case ADJUST_XOR: {
				sourceValue = sourceValue ^ operandValue;
			}	break;
		}

		if (useMultiTarget) {
			// multiple source/target values updating, so store inside loop
			if (!setBufferByte(sourceValue, bufferId, offset + i)) {
				debug_log("bufferAdjust: failed to set result %d at offset %d\n\r", sourceValue, offset + i);
				return;
			}
		}
	}
	if (!useMultiTarget) {
		// single source/target value updating, so store outside loop
		if (!setBufferByte(sourceValue, bufferId, offset)) {
			debug_log("bufferAdjust: failed to set result %d at offset %d\n\r", sourceValue, offset);
			return;
		}
	}
	if (usingCarry) {
		// if we were using carry, store the final carry value
		if (!setBufferByte(carryValue, bufferId, offset + count)) {
			debug_log("bufferAdjust: failed to set carry value %d at offset %d\n\r", carryValue, offset + count);
			return;
		}
	}

	debug_log("bufferAdjust: result %d\n\r", sourceValue);
}

// returns true or false depending on whether conditions are met
// Will read the following arguments from the stream
// operation, checkBufferId; offset; [operand]
// This works in a similar manner to bufferAdjust
// for now, this only supports single-byte comparisons
// as multi-byte comparisons are a bit more complex
// 
bool VDUStreamProcessor::bufferConditional() {
	auto command = readByte_t();
	auto checkBufferId = readWord_t();

	bool useAdvancedOffsets = command & COND_ADVANCED_OFFSETS;
	bool useBufferValue = command & COND_BUFFER_VALUE;
	uint8_t op = command & COND_OP_MASK;
	// conditional operators that are greater than NOT_EXISTS require an operand
	bool hasOperand = op > COND_NOT_EXISTS;

	auto offset = getOffsetFromStream(checkBufferId, useAdvancedOffsets);
	auto operandBufferId = 0;
	auto operandOffset = 0;

	if (useBufferValue) {
		operandBufferId = readWord_t();
		operandOffset = getOffsetFromStream(operandBufferId, useAdvancedOffsets);
	}

	if (command == -1 || checkBufferId == -1 || offset == -1 || operandBufferId == -1 || operandOffset == -1) {
		debug_log("bufferConditional: invalid command, checkBufferId, offset or operand value\n\r");
		return false;
	}

	auto sourceValue = getBufferByte(checkBufferId, offset);
	int16_t operandValue = 0;
	if (hasOperand) {
		operandValue = useBufferValue ? getBufferByte(operandBufferId, operandOffset) : readByte_t();
	}

	debug_log("bufferConditional: command %d, checkBufferId %d, offset %d, operandBufferId %d, operandOffset %d, sourceValue %d, operandValue %d\n\r", command, checkBufferId, offset, operandBufferId, operandOffset, sourceValue, operandValue);

	if (sourceValue == -1 || operandValue == -1) {
		debug_log("bufferConditional: invalid source or operand value\n\r");
		return false;
	}

	bool shouldCall = false;

	switch (op) {
		case COND_EXISTS: {
			shouldCall = sourceValue != 0;
		}	break;
		case COND_NOT_EXISTS: {
			shouldCall = sourceValue == 0;
		}	break;
		case COND_EQUAL: {
			shouldCall = sourceValue == operandValue;
		}	break;
		case COND_NOT_EQUAL: {
			shouldCall = sourceValue != operandValue;
		}	break;
		case COND_LESS: {
			shouldCall = sourceValue < operandValue;
		}	break;
		case COND_GREATER: {
			shouldCall = sourceValue > operandValue;
		}	break;
		case COND_LESS_EQUAL: {
			shouldCall = sourceValue <= operandValue;
		}	break;
		case COND_GREATER_EQUAL: {
			shouldCall = sourceValue >= operandValue;
		}	break;
		case COND_AND: {
			shouldCall = sourceValue && operandValue;
		}	break;
		case COND_OR: {
			shouldCall = sourceValue || operandValue;
		}	break;
	}

	debug_log("bufferConditional: evaluated as %s\n\r", shouldCall ? "true" : "false");

	return shouldCall;
}

// VDU 23, 0, &A0, bufferId; 7: Jump to a buffer
// VDU 23, 0, &A0, bufferId; 9, offset; offsetHighByte : Jump to (advanced) offset within buffer
// Change execution to given buffer (from beginning or at an offset)
//
void VDUStreamProcessor::bufferJump(uint16_t bufferId, uint32_t offset = 0) {
	debug_log("bufferJump: buffer %d\n\r", bufferId);
	if (id == 65535) {
		// we're currently the top-level stream, so we can't jump
		// so have to call instead
		return bufferCall(bufferId, offset);
	}
	if (bufferId == 65535) {
		// buffer ID of -1 (65535) is used as a "null output" stream
		// for a jump it indicates "go to end of stream"
		// this will return out a called stream, offset will be ignored
		auto instream = (MultiBufferStream *)inputStream.get();
		instream->seekTo(instream->size());
		return;
	}
	if (bufferId == id) {
		// this is our current buffer ID, so rewind the stream
		debug_log("bufferJump: rewinding stream\n\r");
		((MultiBufferStream *)inputStream.get())->seekTo(offset);
		return;
	}
	if (buffers.find(bufferId) != buffers.end()) {
		auto streams = buffers[bufferId];
		// replace our input stream with a new one
		auto multiBufferStream = make_shared_psram<MultiBufferStream>(streams);
		if (offset) {
			multiBufferStream->seekTo(offset);
		}
		inputStream = multiBufferStream;
		return;
	}
	debug_log("bufferJump: buffer %d not found\n\r", bufferId);
}

// VDU 23, 0, &A0, bufferId; &0D, sourceBufferId; sourceBufferId; ...; : Copy buffers
// Copy a list of buffers into a new buffer
// list is terminated with a bufferId of 65535 (-1)
// Replaces the target buffer with the new one
// This is useful to construct a single buffer from multiple buffers
// which can be used to construct more complex commands
// Target buffer ID can be included in the source list
//
void VDUStreamProcessor::bufferCopy(uint16_t bufferId) {
	// read list of source buffer IDs
	std::vector<uint16_t> sourceBufferIds;
	auto sourceBufferId = readWord_t();
	while (sourceBufferId != 65535) {
		if (sourceBufferId == -1) return;	// timed out
		sourceBufferIds.push_back(sourceBufferId);
		sourceBufferId = readWord_t();
	}
	// prepare a vector for storing our buffers
	std::vector<std::shared_ptr<BufferStream>, psram_allocator<std::shared_ptr<BufferStream>>> streams;
	// loop thru buffer IDs
	for (auto sourceId : sourceBufferIds) {
		if (buffers.find(sourceId) != buffers.end()) {
			// buffer ID exists
			// loop thru buffers stored against this ID
			for (auto buffer : buffers[sourceId]) {
				// push a copy of the buffer into our vector
				auto bufferStream = make_shared_psram<BufferStream>(buffer->size());
				bufferStream->writeBuffer(buffer->getBuffer(), buffer->size());
				streams.push_back(bufferStream);
			}
		} else {
			debug_log("bufferCopy: buffer %d not found\n\r", sourceId);
		}
	}
	// replace buffer with new one
	buffers[bufferId].clear();
	for (auto buffer : streams) {
		debug_log("bufferCopy: copying stream %d bytes\n\r", buffer->size());
		buffers[bufferId].push_back(buffer);
	}
	debug_log("bufferCopy: copied %d streams into buffer %d (%d)\n\r", streams.size(), bufferId, buffers[bufferId].size());
}

// VDU 23, 0, &A0, bufferId; &0E : Consolidate blocks within buffer
// Consolidate multiple streams/blocks into a single block
// This is useful for using bitmaps sent in multiple blocks
//
void VDUStreamProcessor::bufferConsolidate(uint16_t bufferId) {
	// Create a new stream big enough to contain all streams in the given buffer
	// Copy all streams into the new stream
	// Replace the given buffer with the new stream	
	if (buffers.find(bufferId) != buffers.end()) {
		if (buffers[bufferId].size() == 1) {
			// only one stream, so nothing to consolidate
			return;
		}
		// buffer ID exists
		// work out total length of buffer
		uint32_t length = 0;
		for (auto buffer : buffers[bufferId]) {
			length += buffer->size();
		}
		auto bufferStream = make_shared_psram<BufferStream>(length);
		auto offset = 0;
		for (auto buffer : buffers[bufferId]) {
			auto bufferLength = buffer->size();
			memcpy(bufferStream->getBuffer() + offset, buffer->getBuffer(), bufferLength);
			offset += bufferLength;
		}
		buffers[bufferId].clear();
		buffers[bufferId].push_back(bufferStream);
		debug_log("bufferConsolidate: consolidated %d streams into buffer %d\n\r", buffers[bufferId].size(), bufferId);
	} else {
		debug_log("bufferConsolidate: buffer %d not found\n\r", bufferId);
	}
}

// VDU 23, 0, &A0, bufferId; &0F, length; : Split buffer into blocks
// Split a buffer into multiple blocks/streams
//
void VDUStreamProcessor::bufferSplit(uint16_t bufferId, uint16_t length) {
	if (buffers.find(bufferId) != buffers.end()) {
		// buffer ID exists
		// a split buffer can't be used for a bitmap, so clear it
		clearBitmap(bufferId);
		// consolidate it to simplify splitting
		bufferConsolidate(bufferId);
		// get length of first block
		auto stream =  buffers[bufferId][0];
		// clear the buffer
		buffers[bufferId].clear();
		// work out how many blocks we'll need
		auto totalLength = stream->size();
		auto blockCount = totalLength / length;
		if (totalLength % length > 0) {
			blockCount++;
		}
		// create a new buffer for each block and split the data into each block
		auto remaining = totalLength;
		auto sourceData = stream->getBuffer();
		for (auto i = 0; i < blockCount; i++) {
			auto bufferLength = length;
			if (remaining < bufferLength) {
				bufferLength = remaining;
			}
			auto bufferStream = make_shared_psram<BufferStream>(bufferLength);
			memcpy(bufferStream->getBuffer(), sourceData, bufferLength);
			buffers[bufferId].push_back(bufferStream);
			sourceData += bufferLength;
			remaining -= bufferLength;
		}
		debug_log("bufferSplit: split buffer %d into %d blocks of length %d\n\r", bufferId, blockCount, length);
	} else {
		debug_log("bufferSplit: buffer %d not found\n\r", bufferId);
	}
}

// VDU 23, 0, &A0, bufferId; &10 : Reverse blocks within buffer
// Reverses the order of blocks within a buffer
// may be useful for mirroring bitmaps if they have been split by row
//
void VDUStreamProcessor::bufferReverseBlocks(uint16_t bufferId) {
	if (buffers.find(bufferId) != buffers.end()) {
		// reverse the order of the streams
		std::reverse(buffers[bufferId].begin(), buffers[bufferId].end());
	}
}

void reverseValues(uint8_t * data, uint32_t length, uint8_t valueSize) {
	// get last offset into buffer
	auto bufferEnd = length - valueSize;

	if (valueSize == 1) {
		// reverse the data
		for (auto i = 0; i <= (bufferEnd / 2); i++) {
			auto temp = data[i];
			data[i] = data[bufferEnd - i];
			data[bufferEnd - i] = temp;
		}
	} else {
		// reverse the data in chunks
		for (auto i = 0; i <= (bufferEnd / (valueSize * 2)); i++) {
			auto sourceOffset = i * valueSize;
			auto targetOffset = bufferEnd - sourceOffset;
			for (auto j = 0; j < valueSize; j++) {
				auto temp = data[sourceOffset + j];
				data[sourceOffset + j] = data[targetOffset + j];
				data[targetOffset + j] = temp;
			}
		}
	}
}

// VDU 23, 0, &A0, bufferId; &11, options, <parameters> : Reverse buffer
// Reverses the contents of blocks within a buffer
// may be useful for mirroring bitmaps
//
void VDUStreamProcessor::bufferReverse(uint16_t bufferId, uint8_t options) {
	if (buffers.find(bufferId) != buffers.end()) {
		// buffer ID exists

		bool use16Bit = options & REVERSE_16BIT;
		bool use32Bit = options & REVERSE_32BIT;
		bool useSize  = (options & REVERSE_SIZE) == REVERSE_SIZE;
		bool useChunks = options & REVERSE_CHUNKED;
		bool reverseBlocks = options & REVERSE_BLOCK;
		uint8_t unused = options & REVERSE_UNUSED_BITS;

		// future expansion may include:
		// reverse at an offset for a set length (within blocks)
		// reversing across whole buffer (not per block)

		if (unused != 0) {
			debug_log("bufferReverse: warning - unused bits in options byte\n\r");
		}

		auto valueSize = 1;
		auto chunkSize = 0;

		if (useSize) {
			// size follows as a word
			valueSize = readWord_t();
			if (valueSize == -1) {
				return;
			}
		} else if (use32Bit) {
			valueSize = 4;
		} else if (use16Bit) {
			valueSize = 2;
		}

		if (useChunks) {
			chunkSize = readWord_t();
			if (chunkSize == -1) {
				return;
			}
		}

		// verify that our blocks are a multiple of valueSize
		for (auto buffer : buffers[bufferId]) {
			auto size = buffer->size();
			if (size % valueSize != 0 || (chunkSize != 0 && size % chunkSize != 0)) {
				debug_log("bufferReverse: error - buffer %d contains block not a multiple of value/chunk size %d\n\r", bufferId, valueSize);
				return;
			}
		}

		debug_log("bufferReverse: reversing buffer %d, value size %d, chunk size %d\n\r", bufferId, valueSize, chunkSize);

		for (auto buffer : buffers[bufferId]) {
			if (chunkSize == 0) {
				// no chunking, so simpler reverse
				reverseValues(buffer->getBuffer(), buffer->size(), valueSize);
			} else {
				// reverse in chunks
				auto bufferData = buffer->getBuffer();
				auto bufferLength = buffer->size();
				auto chunkCount = bufferLength / chunkSize;
				for (auto i = 0; i < chunkCount; i++) {
					reverseValues(bufferData + (i * chunkSize), chunkSize, valueSize);
				}
			}
		}

		if (reverseBlocks) {
			// reverse the order of the streams
			bufferReverseBlocks(bufferId);
		}

		debug_log("bufferReverse: reversed buffer %d\n\r", bufferId);
	} else {
		debug_log("bufferReverse: buffer %d not found\n\r", bufferId);
	}
}

#endif // VDU_BUFFERED_H

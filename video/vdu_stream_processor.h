#ifndef VDU_STREAM_PROCESSOR_H
#define VDU_STREAM_PROCESSOR_H

#include <memory>
#include <Stream.h>

#include "agon.h"
#include "types.h"

class VDUStreamProcessor {
	public:
		VDUStreamProcessor(std::shared_ptr<Stream> inputStream, std::shared_ptr<Stream> outputStream, uint16_t bufferId) :
			inputStream(inputStream), outputStream(outputStream), originalOutputStream(outputStream), id(bufferId) {}
		VDUStreamProcessor(Stream *input) :
			inputStream(std::shared_ptr<Stream>(input)), outputStream(inputStream), originalOutputStream(inputStream) {}
		inline bool byteAvailable() {
			return inputStream->available() > 0;
		}
		inline uint8_t readByte() {
			return inputStream->read();
		}
		inline void writeByte(uint8_t b) {
			if (outputStream) {
				outputStream->write(b);
			}
		}
		void send_packet(uint8_t code, uint16_t len, uint8_t data[]);

		void processAllAvailable();
		void processNext();

		void vdu(uint8_t c);

		void wait_eZ80();
		void sendModeInformation();

		uint16_t id = 65535;
	private:
		std::shared_ptr<Stream> inputStream;
		std::shared_ptr<Stream> outputStream;
		std::shared_ptr<Stream> originalOutputStream;

		int16_t readByte_t(uint16_t timeout);
		int32_t readWord_t(uint16_t timeout);
		int32_t read24_t(uint16_t timeout);
		uint8_t readByte_b();
		uint32_t readIntoBuffer(uint8_t * buffer, uint32_t length, uint16_t timeout);
		uint32_t discardBytes(uint32_t length, uint16_t timeout);

		void vdu_colour();
		void vdu_gcol();
		void vdu_palette();
		void vdu_mode();
		void vdu_graphicsViewport();
		void vdu_plot();
		void vdu_resetViewports();
		void vdu_textViewport();
		void vdu_origin();
		void vdu_cursorTab();

		void vdu_sys();
		void vdu_sys_video();
		void sendGeneralPoll();
		void vdu_sys_video_kblayout();
		void sendCursorPosition();
		void sendScreenChar(uint16_t x, uint16_t y);
		void sendScreenPixel(uint16_t x, uint16_t y);
		void sendTime();
		void vdu_sys_video_time();
		void sendKeyboardState();
		void vdu_sys_keystate();
		void vdu_sys_scroll();
		void vdu_sys_cursorBehaviour();
		void vdu_sys_udg(char c);

		void vdu_sys_audio();
		void sendAudioStatus(uint8_t channel, uint8_t status);
		uint8_t loadSample(uint8_t sampleIndex, uint32_t length);
		void setVolumeEnvelope(uint8_t channel, uint8_t type);
		void setFrequencyEnvelope(uint8_t channel, uint8_t type);

		void vdu_sys_sprites(void);
		void receiveBitmap(uint8_t cmd, uint16_t width, uint16_t height);

		void vdu_sys_hexload(void);
		void sendKeycodeByte(uint8_t b, bool waitack);

		void vdu_sys_buffered();
		void bufferWrite(uint16_t bufferId);
		void bufferCall(uint16_t bufferId);
		void bufferClear(uint16_t bufferId);
		void bufferCreate(uint16_t bufferId);
		void setOutputStream(uint16_t bufferId);
		int16_t getBufferByte(uint16_t bufferId, uint32_t offset);
		bool setBufferByte(uint8_t value, uint16_t bufferId, uint32_t offset);
		void bufferAdjust(uint16_t bufferId);
		void bufferConditionalCall(uint16_t bufferId);
};

// Read an unsigned byte from the serial port, with a timeout
// Returns:
// - Byte value (0 to 255) if value read, otherwise -1
//
int16_t VDUStreamProcessor::readByte_t(uint16_t timeout = COMMS_TIMEOUT) {
	auto t = millis();

	while (millis() - t <= timeout) {
		if (byteAvailable()) {
			return readByte();
		}
	}
	return -1;
}

// Read an unsigned word from the serial port, with a timeout
// Returns:
// - Word value (0 to 65535) if 2 bytes read, otherwise -1
//
int32_t VDUStreamProcessor::readWord_t(uint16_t timeout = COMMS_TIMEOUT) {
	auto l = readByte_t(timeout);
	if (l >= 0) {
		auto h = readByte_t(timeout);
		if (h >= 0) {
			return (h << 8) | l;
		}
	}
	return -1;
}

// Read an unsigned 24-bit value from the serial port, with a timeout
// Returns:
// - Value (0 to 16777215) if 3 bytes read, otherwise -1
//
int32_t VDUStreamProcessor::read24_t(uint16_t timeout = COMMS_TIMEOUT) {
	auto l = readByte_t(timeout);
	if (l >= 0) {
		auto m = readByte_t(timeout);
		if (m >= 0) {
			auto h = readByte_t(timeout);
			if (h >= 0) {
				return (h << 16) | (m << 8) | l;
			}
		}
	}
	return -1;
}

// Read an unsigned byte from the serial port (blocking)
//
uint8_t VDUStreamProcessor::readByte_b() {
	while (inputStream->available() == 0);
	return readByte();
}

// Read a given number of bytes from the serial port into a buffer
// Returns number of remaining bytes
// which should be zero if all bytes were read
// but will be non-zero if the read timed out
//
uint32_t VDUStreamProcessor::readIntoBuffer(uint8_t * buffer, uint32_t length, uint16_t timeout = COMMS_TIMEOUT) {
	uint32_t remaining = length;
	auto t = xTaskGetTickCountFromISR();
	auto now = t;
	auto timeCheck = pdMS_TO_TICKS(timeout);

	while (remaining > 0) {
		now = xTaskGetTickCountFromISR();
		if (now - t > timeCheck) {
			debug_log("readIntoBuffer: timed out\n\r");
			return remaining;
		}
		auto available = inputStream->available();
		if (available > 0) {
			if (available > remaining) {
				available = remaining;
			}
			// debug_log("readIntoBuffer: reading %d bytes\n\r", available);
			inputStream->readBytes(buffer, available);
			buffer += available;
			remaining -= available;
			t = now;
		}
	}
	return remaining;
}

// Discard a given number of bytes from input stream
// Returns 0 on success, or the number of bytes remaining if timed out
//
uint32_t VDUStreamProcessor::discardBytes(uint32_t length, uint16_t timeout = COMMS_TIMEOUT) {
	uint32_t remaining = length;
	auto bufferSize = 64;
	auto readSize = bufferSize;
	auto buffer = make_unique_psram_array<uint8_t>(bufferSize);

	while (remaining > 0) {
		if (remaining < readSize) {
			readSize = remaining;
		}
		if (readIntoBuffer(buffer.get(), readSize, timeout) != 0) {
			// timed out
			return remaining;
		}
		remaining -= readSize;
	}
	return remaining;
}

// Send a packet of data to the MOS
//
void VDUStreamProcessor::send_packet(uint8_t code, uint16_t len, uint8_t data[]) {
	writeByte(code + 0x80);
	writeByte(len);
	for (int i = 0; i < len; i++) {
		writeByte(data[i]);
	}
}

// Process all available commands from the stream
//
void VDUStreamProcessor::processAllAvailable() {
	while (byteAvailable()) {
		vdu(readByte());
	}
}

// Process next command from the stream
//
void VDUStreamProcessor::processNext() {
	if (byteAvailable()) {
		vdu(readByte());
	}
}

#include "vdu.h"

#endif // VDU_STREAM_PROCESSOR_H

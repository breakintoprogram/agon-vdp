#include "vdu.h"


class VDUStreamProcessor {
	public:
		VDUStreamProcessor(Stream * stream) : stream(stream) {}
		inline bool byteAvailable() {
			return stream->available() > 0;
		}
		inline uint8_t readByte() {
			return stream->read();
		}
		inline void writeByte(uint8_t b) {
			stream->write(b);
		}
		int16_t readByte_t(uint16_t timeout);
		uint32_t readWord_t(uint16_t timeout);
		uint32_t read24_t(uint16_t timeout);
        uint8_t readByte_b();
        uint32_t readLong_b();
		void discardBytes(uint32_t length);
		void send_packet(uint8_t code, uint16_t len, uint8_t data[]);

		void processAllAvailable();
    private:
        Stream * stream;
};

// Read an unsigned byte from the serial port, with a timeout
// Returns:
// - Byte value (0 to 255) if value read, otherwise -1
//
int16_t VDUStreamProcessor::readByte_t(uint16_t timeout = 1000) {
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
uint32_t VDUStreamProcessor::readWord_t(uint16_t timeout = 1000) {
	auto l = readByte_t();
	if (l >= 0) {
		auto h = readByte_t();
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
uint32_t VDUStreamProcessor::read24_t(uint16_t timeout = 1000) {
	auto l = readByte_t();
	if (l >= 0) {
		auto m = readByte_t();
		if (m >= 0) {
			auto h = readByte_t();
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
  	while (stream->available() == 0);
  	return readByte();
}

// Read an unsigned word from the serial port (blocking)
//
uint32_t VDUStreamProcessor::readLong_b() {
  uint32_t temp;
  temp  =  readByte_b();		// LSB;
  temp |= (readByte_b() << 8);
  temp |= (readByte_b() << 16);
  temp |= (readByte_b() << 24);
  return temp;
}

// Discard a given number of bytes from input stream
//
void VDUStreamProcessor::discardBytes(uint32_t length) {
	while (length > 0) {
		readByte_t(0);
		length--;
	}
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

#ifndef BUFFERS_H
#define BUFFERS_H

#include <memory>
#include <vector>
#include <unordered_map>

#include "buffer_stream.h"

std::unordered_map<uint16_t, std::vector<std::shared_ptr<BufferStream>>> buffers;

#endif // BUFFERS_H

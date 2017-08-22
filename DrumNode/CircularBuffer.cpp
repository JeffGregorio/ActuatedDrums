#include "CircularBuffer.h"

CircularBuffer::CircularBuffer() : write_idx(0) {
  for (int i = 0; i < BUFFER_SIZE; i++)
    buffer[i] = 0.0;
}

CircularBuffer::~CircularBuffer() {}

void CircularBuffer::append(float sample) {
  buffer[write_idx++] = sample;
  if (write_idx == BUFFER_SIZE) 
    write_idx = 0;
}

float CircularBuffer::read(int16_t s_delay) {
  int16_t read_idx = write_idx - s_delay;
  if (read_idx < 0)
    return buffer[BUFFER_SIZE + read_idx];
  else
    return buffer[read_idx];
}

float CircularBuffer::read(float s_delay) {
  int16_t idx_0 = floor(s_delay);
  int16_t idx_1 = ceil(s_delay);
  float del = s_delay - idx_0;
  return ((1-del)*read(idx_0) + del*read(idx_1));
}



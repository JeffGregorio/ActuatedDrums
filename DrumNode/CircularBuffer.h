/**
 * CircularBuffer.h
 * 
 * Simple circular buffer implementation. Allows reading and writing single samples.
 * 
 */

#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H

#include <stdint.h>
#include <math.h>

#define BUFFER_SIZE (1024)   // Buffer size (max sample delay)

class CircularBuffer {

public:
  CircularBuffer();
  ~CircularBuffer();

  void append(float sample);
  float read(int16_t sampleDelay);
  float read(float sampleDelay);

private:

  float buffer[BUFFER_SIZE];
  int16_t write_idx;
};

#endif

/* Oscillator.h
 *  
 *  Wavetable oscillator with floating point output and 32-bit freq resolution. 
 *  Implements fractional indexing in Q11.21 fixed point.
 */

#ifndef OSCILLATOR_H
#define OSCILLATOR_H
 
#include <stdint.h>

#define TAB_LEN 2048      // Must be power of 2
#define IDX_FRAC_RES 21   // Must be (32 - log2(TAB_LEN))

typedef enum WaveShape {
  kWaveShapeSine = 0,
  kWaveShapeSquare,
  kWaveShapeSaw
} WaveShape;

class Oscillator {

public:
  
  Oscillator(float sample_rate, float freq_hz);
  ~Oscillator();

  // Parameter i/o
  void setF0(float f0_Hz);
  void setF0Mod(float mod);
  void setF0ModAmp(float amp);

  void setWaveShape(WaveShape shape);

  // Audio/Control i/o
  float render();
  float value;

private:

  void table_init(); 
  void setF0Norm(float freq);
  void setF0Norm(float f0, float rise_time_ms);

  float sintable[TAB_LEN];      // Wave table (sine)
  float squaretable[TAB_LEN];   // " " (square)
  float sawtable[TAB_LEN];      // " " (sawtooth)

  WaveShape waveShape;          // Current wave shape

  float f0;             // Fundamental freq. value (base + modulation)
  float f0_base;        // " " base value
  float f0_mod;         // " " modulation value
  float f0_mod_amp;     // " " modulation amplitude

  uint32_t accumulator;         // 32-bit phase accumulator, where B+F = 32
  int phase;                    // Phase accumulator increment
  int target_phase;             // Target phase increment (for gliss)
  int phase_slope;              // Slope to reach target phase (per sample)
  int rise_time_samples;        // Gliss time in audio samples

  float fs;         // Audio sample rate
};

#endif

#include "Oscillator.h"
#include <math.h>

Oscillator::Oscillator(float sample_rate, float f0) : 
  fs(sample_rate), f0_mod(0.0), f0_mod_amp(0.0), waveShape(kWaveShapeSine) {
  
  table_init();           // Initialize sine wave table
  setF0(f0);              // Set fundamental frequency in Hz

  accumulator = 0;        // Initialize 32-bit phase accumulator
  value = sintable[0];    // Initialize first wavetable value
}

Oscillator::~Oscillator() { }

/** 
 *  Set the oscillator's frequency in Hz. 
 */
void Oscillator::setF0(float f0_Hz) {
  f0_base = f0_Hz;
  f0 = f0_base + f0_base*f0_mod_amp*f0_mod;
  setF0Norm(f0 / fs);
}

void Oscillator::setF0Mod(float mod) {
  f0_mod = mod;
  f0 = f0_base + f0_base*f0_mod_amp*f0_mod;
  setF0Norm(f0 / fs);
}

void Oscillator::setF0ModAmp(float amp) {
  f0_mod_amp = amp;
  f0 = f0_base + f0_base*f0_mod_amp*f0_mod;
  setF0Norm(f0 / fs);
}

void Oscillator::setWaveShape(WaveShape shape) {
  waveShape = shape;
}

/**
 * Set the oscillator's frequency (normalized), where [0, 0.5) corresponds to [0, f_nyquist). 
 * Phase represents a fraction of the maximum value representable by uint32_t, which is the  
 * combination of B integer and F fractional indices. 
 */
void Oscillator::setF0Norm(float f0) {
  phase = target_phase = f0 * 4294967296. + 0.5;  // 2^32 = 4294967296
  phase_slope = 0;
}

void Oscillator::setF0Norm(float f0, float rise_time_ms) {
  target_phase = f0 * 4294967296. + 0.5;          // 2^32 = 4294967296
  phase_slope = (target_phase - phase) / (rise_time_ms / 1000.0 * fs);
}

/** 
 *  Render and return one oscillator sample.
 */
float Oscillator::render() {
  
  int idx = (accumulator >> IDX_FRAC_RES) & (TAB_LEN-1);  // Remove fractional part of index

  switch (waveShape) {
    case kWaveShapeSquare:
      value = squaretable[idx];
      break;
    case kWaveShapeSaw:
      value = sawtable[idx];
      break;
    case kWaveShapeSine:
    default:
      value = sintable[idx];
      break;  
  }                   

  // Update the phase if we're ramping to a target fundamental frequency
  if ((phase_slope > 0 && phase < target_phase) || (phase_slope < 0 && phase > target_phase))
    phase += phase_slope;
    
  accumulator += phase;     // Increment the phase, exploit 32-bit overflow to wrap back to zero
  return value;
}

/** 
 *  Pre-computer single period wavetables.
 */
void Oscillator::table_init() {
  float two_pi = 6.28318530717959;
  for (int i = 0; i < TAB_LEN; ++i) {
    sintable[i] = sin(two_pi * i / TAB_LEN);
    squaretable[i] = sintable[i] < 0 ? -1.0 : 1.0;
    sawtable[i] = 2*i / (float)TAB_LEN - 1;
  }
}


  

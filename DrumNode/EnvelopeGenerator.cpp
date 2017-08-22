#include "EnvelopeGenerator.h"

EnvelopeGenerator::EnvelopeGenerator(float sampleRate, float atk_ms, float sus, float rel_ms) 
: level(0.0), sustain_level(EGEN_MAX), do_sustain(false), state(kEnvelopeState_Idle), ramp(kEnvelopeRamp_Linear), 
  fs(sampleRate), gate_on_thresh(0.5), gate_off_thresh(0.3), cv_gate(true), was_cv_gated(false), falling_edge(false) {
  setAttackTime(atk_ms);
  setReleaseTime(rel_ms);
}

EnvelopeGenerator::~EnvelopeGenerator() {}

/** 
 * Set attack time in milliseconds. If the envelope is in the attack state, 
 * re-computer the multiplier.
 */
void EnvelopeGenerator::setAttackTime(float atk_ms) {
  atk_time_samples = atk_ms*fs/1000.0;
  if (state == kEnvelopeState_Attack) 
    computeRamp(level, sustain_level, atk_time_samples);
}

void EnvelopeGenerator::setSustainLevel(float sus_level) {
  sustain_level = sus_level;
}

/** 
 * Set release time in milliseconds. If the envelope is in the release state, 
 * re-computer the multiplier.
 */
void EnvelopeGenerator::setReleaseTime(float rel_ms) {
  rel_time_samples = rel_ms*fs/1000.0;
  if (state == kEnvelopeState_Release)
    computeRamp(level, EGEN_MIN, rel_time_samples);
}

/** 
 * Set ramp slope type (linear/exponential)
 */
void EnvelopeGenerator::setRamp(EnvelopeRamp rampMode) {
  ramp = rampMode;
}

/**
 * Whether the envelope sustains until gated off or releases immediately
 * after attack.
 */
void EnvelopeGenerator::setSustain(bool doSustain) {
  do_sustain = doSustain;
  if (state == kEnvelopeState_Sustain) 
    gate(false);
}

/**
 * Set input 'CV' threshold beyond which the generator gates on.
 */
void EnvelopeGenerator::setGateOnThresh(float thresh) {
  gate_on_thresh = thresh;
}


/**
 * Set input 'CV' threshold beyond which the generator gates off and
 * allows re-gating.
 */
void EnvelopeGenerator::setGateOffThresh(float thresh) {
  gate_off_thresh = thresh;
}

/** 
 *  Gate the envelope generator with a 'CV' signal on interval [0.0, 1.0].
 *  - Gate ON if the input CV exceeds gate_on_thresh
 *  - If egen sustains, gate OFF if input CV falls below gate_off_thresh
 *  - Only allow re-gating if the cv has fallen below the gate_off_thresh
 */
void EnvelopeGenerator::gate_cv(float cv) {
  
  if (!cv_gate)
    return;
    
  if (state == kEnvelopeState_Idle && cv > gate_on_thresh) {
    gate(true);
    was_cv_gated = true;
  }
  else if (state != kEnvelopeState_Idle && 
           state != kEnvelopeState_Attack && 
           cv < gate_off_thresh) {
    if (do_sustain && state != kEnvelopeState_Sustain) {
      gate(false);
    }
  }
}

/**
 * Trigger attack (true) or release (false) states and compute the multiplier.
 */
void EnvelopeGenerator::gate(bool on) {
  if (on) {
    was_cv_gated = false;
    state = kEnvelopeState_Attack;
    computeRamp(level, sustain_level, atk_time_samples);  // Current level -> max level
  }
  else {
    state = kEnvelopeState_Release;
    computeRamp(level, EGEN_MIN, rel_time_samples);  // Current level -> min level
  }
}

/**
 * Compute the slope or multiplier to reach level a1 from level a0 in the specified
 * number of samples, depending on the current ramp mode.
 */
void EnvelopeGenerator::computeRamp(float a0, float a1, float duration_samples) {
  switch (ramp) {
    case kEnvelopeRamp_Linear:
      linear_slope = (a1 - a0) / duration_samples;
      break;
    case kEnvelopeRamp_Exponential:
      exponential_multiplier = 1 + (log(a1) - log(a0)) / duration_samples;
      break;
  }
}

/**
 * Render one sample of the current envelope level. Automatically enter sustain 
 * state from attack if we've reached the maximum level 2^ENV_RES. Automatically 
 * enter idle state from release if we've reached the minimum level.
 */
float EnvelopeGenerator::render() {
  
	switch (state) {
		case kEnvelopeState_Attack:
      falling_edge = false;
      updateLevel();
      if (level > sustain_level) {
        level = sustain_level;
        if (do_sustain)
          state = kEnvelopeState_Sustain;
        else
          gate(false);
      }
			break;
		case kEnvelopeState_Release:
      updateLevel();
      if (level <= EGEN_MIN) {
        level = EGEN_MIN;
        state = kEnvelopeState_Idle;
      }
      if (level < sustain_level/2 && level_previous > sustain_level/2) 
        falling_edge = true;
			break;
		case kEnvelopeState_Sustain:
      level = sustain_level;
      break;
		case kEnvelopeState_Idle:
      level = 0.0;
      break;
	}
	return level;
}

void EnvelopeGenerator::updateLevel() {
  level_previous = level;
  switch (ramp) {
    case kEnvelopeRamp_Linear:
      level += linear_slope;
      break;
    case kEnvelopeRamp_Exponential:
      level *= exponential_multiplier;
      break;
  }
}


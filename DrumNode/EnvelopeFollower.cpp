#include "EnvelopeFollower.h"

EnvelopeFollower::EnvelopeFollower(float sampleRate, float atk_ms, float rel_ms) 
: fs(sampleRate), value(0.0f) {
  setAttackTime(atk_ms);
  setReleaseTime(rel_ms);
}

EnvelopeFollower::~EnvelopeFollower() {}

void EnvelopeFollower::setAttackTime(float atk_ms) {
  atk_tau = exp(-1 / (fs*atk_ms/1000.0f));
}

void EnvelopeFollower::setReleaseTime(float rel_ms) {
  rel_tau = exp(-1 / (fs*rel_ms/1000.0f));
}

float EnvelopeFollower::process(float sample) {
  
  sample = fabs(sample); 

  if(value < sample) 
    value = sample + atk_tau*(value-sample);
  else 
    value = sample + rel_tau*(value-sample);

  return SCALE*value;
}


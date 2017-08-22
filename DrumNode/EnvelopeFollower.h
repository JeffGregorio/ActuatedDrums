/* EnvelopeFollower.h
 *  
 *  Floating point envelope follower with variable attack and release times.
 */

#ifndef ENVELOPEFOLLOWER_H
#define ENVELOPEFOLLOWER_H

#include <stdlib.h>
#include <math.h>

#define SCALE (4.0)

class EnvelopeFollower {

public:

  EnvelopeFollower(float sampleRate, float atk_ms, float rel_ms);
  ~EnvelopeFollower();

  // Parameter i/o
  void setAttackTime(float atk_ms);
  void setReleaseTime(float rel_ms);

  // Audio/Control i/o
  float process(float sample);
  float getValue()  { return value; }

private:

  float fs;         // Sample rate

  float atk_tau;    // Attack time constant
  float rel_tau;    // Release time constant

  float value;      // Current envelope follower value
};

#endif

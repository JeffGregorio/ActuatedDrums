/* EnvelopeGenerator.h
 *  
 * Floating-point linear/exponential envelope generator with variable attack and release time.
 * 
 */

#ifndef ENVELOPGENERATOR_H
#define ENVELOPGENERATOR_H

#include <Math.h>

#define EGEN_MAX (1.0)
#define EGEN_MIN (0.001)

typedef enum EnvelopeRamp {
  kEnvelopeRamp_Linear = 0,
  kEnvelopeRamp_Exponential
} EnvelopeRamp;

typedef enum EnvelopeState {
	kEnvelopeState_Idle = 0,
	kEnvelopeState_Attack,
	kEnvelopeState_Sustain,
	kEnvelopeState_Release
} EnvelopeState;

class EnvelopeGenerator {

public:

	EnvelopeGenerator(float sampleRate, float atk, float sus, float rel);
	~EnvelopeGenerator();

	void setAttackTime(float atk_ms);
  void setSustainLevel(float sus_level);   
	void setReleaseTime(float rel_ms);
  void setRamp(EnvelopeRamp ramp);   
  void setSustain(bool doSustain);
  void setGateOnThresh(float thresh); 
  void setGateOffThresh(float thresh);

  void gate(bool on);
  void gate_cv(float cv);
  float render();

  float getLevel()  { return level; } 
  float getSustain() { return sustain_level; }
  bool doesSustain()  { return do_sustain; }
  EnvelopeState getState() { return state; }

  bool falling_edge;    // Has release state level dropped below env_max/2?
  // - Public so the main loops can check for falling edge to trigger listeners,
  //   and reset to false after the listener messages have been sent.
  bool was_cv_gated;

private:
  
  void updateLevel();
  void computeRamp(float a0, float a1, float dur_s);
  
  float atk_time_samples;
  float rel_time_samples;

  float level;
  float level_previous;
  float sustain_level;
  bool do_sustain;      
  EnvelopeState state;
 
  EnvelopeRamp ramp;
  float exponential_multiplier;
  float linear_slope;
  
  float fs;         // Audio sample rate

  float gate_on_thresh;
  float gate_off_thresh;
  bool cv_gate;
};

#endif

/**
 * TO DO:
 *  - Propagation mode inspired by wave mechanics
 *    - Propagation is bidirectional
 *    - If a drum that is propagating receives a propagation message, the amplitude of the 
 *      received message should add to amplitude of the current message
 *        - But the next propagation message should send the original amplitude, decayed
 *          so clashes don't lead to growing amplitudes
 *    - Can we have negative amplitudes?
 *      - Absolute value can be used to set sustain level
 *      - Sign can be used for a timbral/color transformation
 *    - Possibly better method:
 *      - Each drum has an LFO with amplitude modulated by an EGEN
 *      - A hit sets the phase of the LFO to zero and starts the EGEN with sustain = 1
 *        - After the first LFO zero-crossing, the message propagates with a reduced 
 *          sustain level
 *        - Result: LFO rate controls propagation rate, envelope times control resonance
 *          time per drum
 *   - Propagation of parameters other than sustain level
 *      - Can we generalize propagation? Enabling configuring the propagation message 
 *        to send a specified paraemter
 *          - Currently implemented propagation mode can be re-configured by enabling
 *            propagation to send /mod/egen/sus_level
 *      - This could enable hits to propagate parameter changes while the henge is played
 *        by a MIDI controller with all-on allocation
 *          - Try propagating LFO rate
*/

#include <IPAddress.h>
#include <OSCMessage.h>
#include <SLIPEncodedSerial.h>
// OSCMessage and SLIPEncodedSerial availab at https://github.com/sandeepmistry/esp8266-OSC

#include <ADC.h>
#include "EnvelopeFollower.h"
#include "EnvelopeGenerator.h"
#include "Oscillator.h"
#include "CircularBuffer.h"
#include "NodeListenerArray.h"

#define PHASE_INVERT
//#define CV1_INVERT
//#define CV2_INVERT
//#define CV3_INVERT
#define BAUD_RATE_DEBUG (230400)
#define BAUD_RATE_ESP (230400)
//#define DEBUG_PRINT

/* Pin assignments */
const int ADC_AUDIO = A0;           // Audio input
const int ADC_CV1 = A17;
const int ADC_CV2 = A18;
const int ADC_CV3 = A3;
const int DAC = A21;                // Audio Output
const int LED_OSC = 13;
const int LED_FOLLOWER = 3;
const int LED_LFO = 4;
const int LED_EGEN = 5;
const int LED_G = 10;
const int LED_R = 29;
const int LED_B = 30;
const int MUTE_CH1 = 2;     // Power amplifier CH1 mute signal

/* I/O Resolution */
const int adc_res = 10;                 // ADC resolution
const int adc_max = (1 << adc_res);
const int adc_half = adc_max / 2;
const int dac_res = 12;                 // DAC resolution
const int dac_max = (1 << dac_res);
const int dac_half = dac_max / 2;
const int adc_scale = dac_res - adc_res;  // DAC res > ADC res

/* Audio sampling */
ADC *adc = new ADC();
IntervalTimer audio_sample_timer;
const float fs = 8000.0;               // Audio sample rate
const float ts_us = 1 / fs * 1e6;       // " " period (microseconds)

/* Audio rate objects */
EnvelopeFollower follower = EnvelopeFollower(fs, 100.0, 100.0); // Env. Follower
EnvelopeGenerator egen = EnvelopeGenerator(fs, 100.0, 1.0, 100.0);
Oscillator lfo = Oscillator(fs, 2.0);
Oscillator vco = Oscillator(fs, 60.0);
CircularBuffer delayBuffer = CircularBuffer();

// ADC/DAC ints
int32_t adc_sample;       // Audio input
int16_t dac_sample;       // Audio output
int16_t adc_cv1;          // CVs from onboard pots
int16_t adc_cv2;          // "
int16_t adc_cv3;          // "
bool cv1_enable = true;
bool cv2_enable = true;
bool cv3_enable = true;

// Sample/control floats
float aud_sample;           // Audio input
float vco_sample;           // Audio rate oscillator
float out_sample;           // Audio input/oscillator mix
float follower_sample;      // Envelope follower
float egen_sample;          // Envelope generator
float lfo_sample;           // LFO (amplitude modulator)
float sample_delay = 10.0;  // Sample delay 

// Misc. effect parameters (note encapsulated in a synth class)
bool follower_gate_enabled = true;  // Whether the EGEN can be triggered by the input follower
bool kill_received = false;     // Temporarily stop propagation without disabling entirely
bool propagate_enabled = false; // Enables starting/continuing propagation on EGEN falling edge
bool propagating = false;       // Whether we're starting or continuing propagation on falling edge
bool propagate_reflect = false; // Whether this node is a reflector (propagate back to source)
float propagate_sus_level;      // Sustain level received by propagate message
float previous_sus_level;       // Allows resetting EGEN sustain level after propgation message passes
float propagate_decay = 0.25;   // Slope of sustain level between propgation messages

float synth_vca_lfo_mod = 0.0;      // Modulation depth [0.0, 1.0]
float fb_gain = 1.0;                // Audio input signal gain
float fb_vca_lfo_mod = 0.0;         // Modulation depth [0.0, 1.0]
float fb_vca_env_mod = 1.0;         // Modulation depth [0.0, 1.0]
float fb_mix = 0.0;                 // Audio input/oscillator mix [0.0, 1.0]

/* OSC */
IPAddress ip_local;
IPAddress ip_multi;
IPAddress remote_ip;
IPAddress gate_remote_ip;
int port_local;
int port_multi;
NodeListenerArray listener_array = NodeListenerArray();

OSCMessage incoming_msg;
OSCMessage set_dest_out("/set_dest");
OSCMessage propagate_out("/propagate");

/* Incoming OSC Handling (via SLIP Serial) */
SLIPEncodedSerial SLIPSerial(Serial3);
IntervalTimer osc_control_timer;
const float fo = 16000.0;                // OSC sample rate
const float to_us = 1 / fo * 1e6;       // " " period (microseconds)

/* Setup */
void setup() {

  Serial.begin(BAUD_RATE_DEBUG);        // Debug serial
  SLIPSerial.begin(BAUD_RATE_ESP);      // OSC serial via ESP8266

  pinMode(ADC_AUDIO, INPUT);            // Audio i/o
  analogWriteResolution(dac_res);
  pinMode(DAC, OUTPUT);

  pinMode(ADC_CV1, INPUT);              // CVs
  pinMode(ADC_CV2, INPUT);
  pinMode(ADC_CV3, INPUT);
  
  pinMode(LED_OSC, OUTPUT);             // LEDs
  pinMode(LED_FOLLOWER, OUTPUT);
  pinMode(LED_LFO, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  pinMode(MUTE_CH1, OUTPUT);            // Control output
  digitalWrite(MUTE_CH1, LOW);

  // Start audio and OSC interval timers
  audio_sample_timer.begin(audio_sample_interrupt, ts_us);
  osc_control_timer.begin(handle_slip_osc, to_us);
}

/* -------------------------------------- */
/* === Main Audio Processing Callback === */
/* -------------------------------------- */
void audio_sample_interrupt() {

  /* === Audio feedback === */
  // Audio input
  adc_sample = adc->analogRead(ADC_AUDIO);  // [0, adc_max]
#ifdef PHASE_INVERT
  adc_sample = adc_max - adc_sample;
#endif
  adc_sample -= adc_half;                     // [-adc_half, adc_half]
  aud_sample = adc_sample / (float)adc_half;  // [-1.0, 1.0]
  aud_sample *= fb_gain;

  // Envelope follower/generator
  follower_sample = follower.process(aud_sample);   // [0.0, 1.0]
  if (follower_gate_enabled) egen.gate_cv(follower_sample);
  egen_sample = egen.render();

  // Audio sample delay
  delayBuffer.append(aud_sample);                 // Append current output sample
  aud_sample = delayBuffer.read(sample_delay);    // Read delayed sample

  // LFO amplitude modulation
  lfo_sample = lfo.render();              // [-1.0, 1.0]
  aud_sample = fb_vca_lfo_mod * lfo_sample * aud_sample + (1.0 - fb_vca_lfo_mod) * aud_sample;

  // EGEN modulation
  aud_sample = fb_vca_env_mod * egen_sample * aud_sample + (1.0 - fb_vca_env_mod) * aud_sample;

  /* === Synthesis === */
  // Oscillator
  vco_sample = vco.render();    // [-1.0, 1.0]
  vco_sample *= egen_sample;

  // LFO amplitude modulation
  vco_sample = synth_vca_lfo_mod * lfo_sample * vco_sample + (1.0 - synth_vca_lfo_mod) * vco_sample;

  // Audio/Oscillator mix
  out_sample = fb_mix * aud_sample + (1.0 - fb_mix) * vco_sample;  
  dac_sample = dac_half * (out_sample + 1.0);
  analogWrite(DAC, dac_sample);           // Output
}

void loop() {

  // Look for falling edge on the envelope generator to trigger listeners
  if (egen.falling_edge) 
    falling_edge();

//  process_cv_1();
//  process_cv_2();
  process_cv_3();

//  lfo.setF0Mod(egen_sample);      // EGEN-->LFO mod
//  vco.setF0Mod(lfo_sample);       // LFO--->VCO mod

  int s_f = 1023 * follower_sample;
  int s_l = 512 * (lfo_sample + 1.0);
  int s_g = egen_sample * 1023;

  analogWrite(LED_FOLLOWER, s_f);
  analogWrite(LED_LFO, s_l);
  analogWrite(LED_EGEN, s_g);

  if (egen.was_cv_gated) {
    analogWrite(LED_R, s_g);        // LED Bulb
    analogWrite(LED_G, s_g);
    analogWrite(LED_B, s_g);
  }
  else {
    analogWrite(LED_R, s_g / (1.0-synth_vca_lfo_mod));        // LED Bulb
    analogWrite(LED_G, s_g / 2.0 / (1.0-synth_vca_lfo_mod));
    analogWrite(LED_B, s_g / 4.0);
  }
}

/**
 * EGEN falling edge handler.
 * 
 * Allows the falling edge of the envelope generator to trigger OSC events including
 * propagation messages to OSC listeners.
 */
void falling_edge() {

  egen.falling_edge = false;    // Reset flag indicating falling edge was handled
  if (egen.was_cv_gated) {
    gate_remote_ip = IPAddress(0, 0, 0, 0);
    egen.was_cv_gated = false;
    propagating = false;
  }

  // Don't propagate if disabled or a kill message was sent
  if (!propagate_enabled || kill_received) {
    kill_received = false;
    propagating = false;
    egen.falling_edge = false;
    return;
  }

  float outgoing_prop_sus;
  
  // Set propagation message with with amplitude (egen sustain level) at max
  if (!propagating) 
    outgoing_prop_sus = 1.0;
  else {  // Set propagation message with current amplitude minus decay
    outgoing_prop_sus = fmax(0.0, propagate_sus_level - propagate_decay); 
    egen.setSustainLevel(previous_sus_level);   // Restore previous sustain level 
  }

  propagating = false;

  // Only propagate if the sustain level exceeds a threshold
  if (outgoing_prop_sus > 0.05) {
    propagate_out.set(0, outgoing_prop_sus);
    
    // Send propagation message back to the source
    if (propagate_reflect) {
      Serial.println("propagate_reflect");
      for (int j = 0; j < 4; j++) {
        set_dest_out.set(j, (int)gate_remote_ip[j]);
        Serial.println((int)gate_remote_ip[j]);
      }
      set_dest_out.set(4, port_local);
      Serial.println(port_local);
      slip_send(set_dest_out);
      slip_send(propagate_out);
    }
    // Send propagation message to any listeners, excluding the propagation source 
    else {  
      Serial.println("!propagate_reflect");
      for (int i = 0; i < listener_array.count(); i++) {
        if (listener_array[i].ip != gate_remote_ip) {
          for (int j = 0; j < 4; j++)
            set_dest_out.set(j, listener_array[i].ip[j]);
          set_dest_out.set(4, listener_array[i].port);
          slip_send(set_dest_out);
          slip_send(propagate_out);
        }
      } 
    }
  }
}

/* === CV1 === */
void process_cv_1() {
  adc_cv1 = adc->analogRead(ADC_CV1);     // [0, adc_max]
#ifdef CV1_INVERT
  adc_cv1 = adc_max - adc_cv1;
#endif
  float new_val = adc_cv1 / (float)adc_max;       // [0, 1.0]
}

/* === CV2 === */
void process_cv_2() {
  adc_cv2 = adc->analogRead(ADC_CV2);
#ifdef CV2_INVERT
  adc_cv2 = adc_max - adc_cv2;
#endif
  float new_val = adc_cv2 / (float)adc_max;
}

/* === CV3: EGEN Follower Gate Threshold === */
void process_cv_3() {
  adc_cv3 = adc->analogRead(ADC_CV3);
#ifdef CV3_INVERT
  adc_cv3 = adc_max - adc_cv3;
#endif
  float new_val = adc_cv3 / (float)adc_max;
  float on_thresh = new_val * 2.0;
  float off_thresh = min(on_thresh - 0.5, 0.2);
  egen.setGateOnThresh(on_thresh);
  egen.setGateOffThresh(off_thresh);
}

/* === SLIP Serial Handling === */
void handle_slip_osc() {

  // At the end of SLIP Serial packets, route OSC messages to their handlers
  if (SLIPSerial.endofPacket()) {
    if (!incoming_msg.hasError()) {
      digitalWrite(LED_OSC, HIGH);
      handle_osc(incoming_msg);       // Pass to main OSC message handler
    }
    incoming_msg.empty();             // Clear OSC data
    incoming_msg.setAddress(NULL);    // Clear OSC path
  }
  // Otherwise, populate an OSC message using SLIP serial bytes
  else {
    digitalWrite(LED_OSC, LOW);
    int n_bytes = SLIPSerial.available();
    while (n_bytes--)
      incoming_msg.fill(SLIPSerial.read());
  }
}

void handle_osc(OSCMessage &msg) {
  char path[128];
  incoming_msg.getAddress(path);

#ifdef DEBUG_PRINT
  handle_test(incoming_msg);
#endif

  /* Messages that should originate from the ESP8266 */
  if (strcmp(path, "/debug") == 0) handle_debug(incoming_msg);
  else if (strcmp(path, "/set_port/local") == 0) handle_set_port_local(incoming_msg);
  else if (strcmp(path, "/set_port/multi") == 0) handle_set_port_multi(incoming_msg);
  else if (strcmp(path, "/remote_ip") == 0) handle_remote_ip(incoming_msg);
  /* Messages that should originaate from the central controller */
  else if (strcmp(path, "/add_listener") == 0) listener_array.handle_add_listener(incoming_msg);
  else if (strcmp(path, "/remove_listener") == 0) listener_array.handle_remove_listener(incoming_msg);
  else if (strcmp(path, "/remove_listeners") == 0) listener_array.handle_remove_listeners(incoming_msg);
  /* Messages that may originate from the central controller or other modules */
  else if (strcmp(path, "/test") == 0) handle_test(incoming_msg);
  else if (strcmp(path, "/mute") == 0) handle_mute(incoming_msg);
  else if (strcmp(path, "/note") == 0) handle_note(incoming_msg);
  else if (strcmp(path, "/propagate") == 0) handle_propagate(incoming_msg);
  else if (strcmp(path, "/propagate/decay") == 0) handle_propagate_decay(incoming_msg);
  else if (strcmp(path, "/propagate/enable") == 0) handle_propagate_enable(incoming_msg);
  else if (strcmp(path, "/propagate/kill") == 0) handle_propagate_kill(incoming_msg);
  else if (strcmp(path, "/propagate/reflect") == 0) handle_propagate_reflect(incoming_msg);
  else if (strcmp(path, "/mod/lfo/wave_shape") == 0) mod_handle_lfo_wave_shape(incoming_msg);
  else if (strcmp(path, "/mod/lfo/rate") == 0) mod_handle_lfo_rate(incoming_msg);
  else if (strcmp(path, "/mod/lfo/env_mod") == 0) mod_handle_lfo_env_mod(incoming_msg);
  else if (strcmp(path, "/mod/egen/atk_time") == 0) mod_handle_egen_atk(incoming_msg);
  else if (strcmp(path, "/mod/egen/sus_level") == 0) mod_handle_egen_sus(incoming_msg);
  else if (strcmp(path, "/mod/egen/rel_time") == 0) mod_handle_egen_rel(incoming_msg);
  else if (strcmp(path, "/mod/egen/do_sus") == 0) mod_handle_egen_do_sus(incoming_msg);
  else if (strcmp(path, "/mod/egen/follower_gate") == 0) mod_handle_egen_follower_gate(incoming_msg);
  else if (strcmp(path, "/mod/egen/gate") == 0) mod_handle_egen_gate(incoming_msg);
  else if (strcmp(path, "/synth/vco/wave_shape") == 0) synth_handle_vco_wave_shape(incoming_msg);
  else if (strcmp(path, "/synth/vco/freq") == 0) synth_handle_vco_freq(incoming_msg);
  else if (strcmp(path, "/synth/vco/lfo_mod") == 0) synth_handle_vco_lfo_mod(incoming_msg);
  else if (strcmp(path, "/synth/vca/lfo_mod") == 0) synth_handle_vca_lfo_mod(incoming_msg);
  else if (strcmp(path, "/fb/gain") == 0) fb_handle_gain(incoming_msg);
  else if (strcmp(path, "/fb/phase") == 0) fb_handle_phase(incoming_msg);
  else if (strcmp(path, "/fb/vca/lfo_mod") == 0) fb_handle_vca_lfo_mod(incoming_msg);
  else if (strcmp(path, "/fb/vca/env_mod") == 0) fb_handle_vca_env_mod(incoming_msg);
  else if (strcmp(path, "/mixer/synth_feedback_mix") == 0) mixer_handle_synth_feedback_mix(incoming_msg);
}

/**
 * /debug "s" <debug_str>
 * 
 * Debug messages from the ESP for printing to Serial monitor through the Teensy.
 */
void handle_debug(OSCMessage &msg) {
  int len = msg.getDataLength(0);
  if (msg.isString(0)) {
    char str[len];
    msg.getString(0, str, len);
    Serial.print("ESP DEBUG:\t");
    Serial.println(str);
  }
}

/**
 * /set_port/local "iiiii" <ip_1><ip_2><ip_3><ip_4><port#>
 * 
 * Local IP and port number for UDP port opened by the ESP8266.
 */
void handle_set_port_local(OSCMessage &msg) {
  if (msg.isInt(0) && msg.isInt(1) && msg.isInt(2) && msg.isInt(3) && msg.isInt(4)) {
    uint8_t ip_bytes[4];
    for (int i = 0; i < 4; i++) 
      ip_bytes[i] = msg.getInt(i);
    ip_local = IPAddress(ip_bytes);
    port_local = msg.getInt(4);
    Serial.print("UDP port (local):\t");
    Serial.print(ip_local);
    Serial.print("\t [");
    Serial.print(port_local);
    Serial.println("]");
  }
}

/**
 * /set_port/multi "iiiii" <ip_1><ip_2><ip_3><ip_4><port#>
 * 
 * Multicast IP and port number for UDP port opened by the ESP8266.
 */
void handle_set_port_multi(OSCMessage &msg) {
  if (msg.isInt(0) && msg.isInt(1) && msg.isInt(2) && msg.isInt(3) && msg.isInt(4)) {
    uint8_t ip_bytes[4];
    for (int i = 0; i < 4; i++) 
      ip_bytes[i] = msg.getInt(i);
    ip_multi = IPAddress(ip_bytes);
    port_multi = msg.getInt(4);
    Serial.print("UDP port (multi):\t");
    Serial.print(ip_multi);
    Serial.print("\t [");
    Serial.print(port_multi);
    Serial.println("]");
  }
}

/**
 * /remote_ip "iiii" <byte1><byte2><byte3><byte4>
 * 
 * Set the most recent remote IP address.
 */
void handle_remote_ip(OSCMessage &msg) {
  byte ip_bytes[4];
  if (msg.isInt(0) && msg.isInt(1) && msg.isInt(2) && msg.isInt(3)) {
    for (int i = 0; i < 4; i++) 
      ip_bytes[i] = msg.getInt(i);
    remote_ip = IPAddress(ip_bytes);
    Serial.print("Setting remote IP = ");
    Serial.print(ip_bytes[0]);
    Serial.print('.');
    Serial.print(ip_bytes[1]);
    Serial.print('.');
    Serial.print(ip_bytes[2]);
    Serial.print('.');
    Serial.println(ip_bytes[3]);
  }
}

/**
 * /test "*+" <varargs>
 * 
 * Sanity checks.
 */
void handle_test(OSCMessage &msg) {

  char path[128];
  incoming_msg.getAddress(path);

  int len;
  char str[128];
  char *chr;
  
  Serial.print(path);
  int msg_size = msg.size();
  for (int i = 0; i < msg_size; i++) {
    Serial.print("\t");
    char type = msg.getType(i);
    switch (type) {
      case 'i':
        Serial.print(msg.getInt(i));
        break;
      case 't':
        break;
      case 'f':
        Serial.print(msg.getFloat(i));
        break;
      case 'd':
        Serial.print(msg.getDouble(i));
        break;
      case 'c':
        msg.getString(i, chr, 1);
        Serial.print(chr);
        break;
      case 's':
        msg.getString(i, str, len);
        Serial.print(str);
        break;
      case 'T':
      case 'F':
        Serial.print(msg.getBoolean(i) ? "true" : "false");
        break;
    }
  }
  Serial.println("");
}

/**
 * /mute "i" <do_mute>
 * 
 * Mute the primary power amplifier channel (CH 1) by sending LOW/HIGH to the LM1876
 * mute pin.
 */
void handle_mute(OSCMessage &msg) {
  if (msg.isInt(0))
    digitalWrite(MUTE_CH1, msg.getInt(0) == 0 ? LOW : HIGH);
}

/**
 * /note "ii" <num><vel>
 * 
 * MIDI-ish note on/off message containing note number and velocity.
 */
void handle_note(OSCMessage &msg) {
  if (msg.isInt(0) && msg.isInt(1)) {
    if (msg.getInt(1) == 0)     // Note OFF
      egen.gate(false);
    else {                      // Note ON
      int nn = msg.getInt(0);
      int vel = msg.getInt(1);
      float f0 = pow(2, (nn - 69) / 12.0) * 440.0;
      vco.setF0(f0);
      egen.setSustainLevel(vel / 127.0);
      egen.gate(true);
      gate_remote_ip = remote_ip;
      propagating = false;
    }
  }
}

/**
 * /propagate "f" <level>
 * 
 * Sets the envelope generator's sustain level and gates it on. Sets a flag for the EGEN 
 * falling edge handler to continue the propagation message. Make a copy of the most recent 
 * remote IP address (the node that sent this propagate message) so we can avoid propagating
 * back in its direction.
 */
void handle_propagate(OSCMessage &msg) {
  if (msg.isFloat(0)) {
    propagate_sus_level = msg.getFloat(0);
    previous_sus_level = egen.getSustain();   
    egen.setSustainLevel(propagate_sus_level);
    egen.gate(true);
    
    if (propagating && remote_ip != gate_remote_ip)
      gate_remote_ip = IPAddress(0, 0, 0, 0);
    else
      gate_remote_ip = remote_ip;
      
    if (!propagating) 
      propagating = true;   // Set flag to continue propagation on egen falling edge
  }
}

/**
 * /propagate/decay "f" <decay>
 * 
 * Sets a constant to subtract from the sustain level when propgating.
 */
void handle_propagate_decay(OSCMessage &msg) {
  if (msg.isFloat(0)) 
    propagate_decay = msg.getFloat(0);
}

/**
 * /propagate/enable "i" <do_enable>
 */
void handle_propagate_enable(OSCMessage &msg) {
  if (msg.isInt(0))
    propagate_enabled = msg.getInt(0)==1;       
}

/**
 * /propagate/kill
 * 
 * Stop propagation without disabling propagation mode.
 */
void handle_propagate_kill(OSCMessage &msg) {
  kill_received = true;
  egen.gate(false);
}

void handle_propagate_reflect(OSCMessage &msg) {
  if (msg.isInt(0)) 
    propagate_reflect = msg.getInt(0)==1;
}

/* ----------------------------- */
/* === Modulation parameters === */
/* ----------------------------- */

/**
 * /mod/lfo/wave_shape "s" <shape>
 * 
 * Set LFO waveform {"sine", "square", "saw"}
 */
void mod_handle_lfo_wave_shape(OSCMessage &msg) {
  int len = msg.getDataLength(0);
  if (msg.isString(0) && len < 8) {
    char shape[len];
    msg.getString(0, shape, len);
    if (strcmp(shape, "sine") == 0)
      lfo.setWaveShape(kWaveShapeSine);
    else if (strcmp(shape, "square") == 0)
      lfo.setWaveShape(kWaveShapeSquare);
    else if (strcmp(shape, "saw") == 0)
      lfo.setWaveShape(kWaveShapeSaw);
  }
}

/**
 * /mod/lfo/rate "f" <rate>
 * 
 * Set LFO rate in Hz.
 */
void mod_handle_lfo_rate(OSCMessage &msg) {
  if (msg.isFloat(0)) {
    lfo.setF0(msg.getFloat(0));
    Serial.print("-- ");
    Serial.println(msg.getFloat(0));
  }
}

/**
 * /mod/lfo/env_mod "f" <amount>
 * 
 * Scalar dictating how much the envelope generator modulates the LFO rate.
 */
void mod_handle_lfo_env_mod(OSCMessage &msg) {
  if (msg.isFloat(0))
    lfo.setF0ModAmp(msg.getFloat(0));
}

/**
 * /mod/egen/atk_time "f" <time>
 * 
 * Envelope generator attack time in seconds.
 */
void mod_handle_egen_atk(OSCMessage &msg) {
  if (msg.isFloat(0))
    egen.setAttackTime(msg.getFloat(0));
}

/**
 * /mod/egen/sus_level "f" <level>
 * 
 * Envelope generator sustain level (recommended [0, 1])
 */
void mod_handle_egen_sus(OSCMessage &msg) {
  if (msg.isFloat(0))
    egen.setSustainLevel(msg.getFloat(0));
}

/**
 * /mod/egen/rel_time "f" <time>
 * 
 * Envelope generator release time in seconds.
 */
void mod_handle_egen_rel(OSCMessage &msg) {
  if (msg.isFloat(0))
    egen.setReleaseTime(msg.getFloat(0));
}

/**
 * /mod/egen/do_sus "i" <do_sus>
 * 
 * Whether the envelope generator sustains between gate/note on and gate/note off, or
 * releases immediately. 
 */
void mod_handle_egen_do_sus(OSCMessage &msg) {
  if (msg.isInt(0))
    egen.setSustain(msg.getInt(0) != 0);
}

/**
 * /mod/egen/follower_gate "i" <do_gate>
 * 
 * Whether the envelope generator can be gated when the envelope follower's signal level
 * excedes the threshold set by the module's threshold calibration CV.
 */
void mod_handle_egen_follower_gate(OSCMessage &msg) {
  if (msg.isInt(0))
    follower_gate_enabled = msg.getInt(0) == 1;
}

/**
 * /mod/egen/gate "i" <gate_on>
 * 
 * Gate the envelope generator on (start attack phase) or off (start release phase).
 */
void mod_handle_egen_gate(OSCMessage &msg) {
  if (msg.isInt(0)) {
    egen.gate(msg.getInt(0) != 0);
    gate_remote_ip = remote_ip;
    propagating = false;
  }
}

/* ------------------------ */
/* === Synth Parameters === */
/* ------------------------ */

/**
 * /synth/vco/wave_shape "s" <shape>
 * 
 * Set VCO waveform {"sine", "square", "saw"}
 */
void synth_handle_vco_wave_shape(OSCMessage &msg) {
  int len = msg.getDataLength(0);
  if (msg.isString(0) && len < 8) {
    char shape[len];
    msg.getString(0, shape, len);
    if (strcmp(shape, "sine") == 0)
      vco.setWaveShape(kWaveShapeSine);
    else if (strcmp(shape, "square") == 0)
      vco.setWaveShape(kWaveShapeSquare);
    else if (strcmp(shape, "saw") == 0)
      vco.setWaveShape(kWaveShapeSaw);
  }
}

/**
 * /synth/vco/freq "f" <freq>
 * 
 * Set the VCO frequency in Hz.
 */
void synth_handle_vco_freq(OSCMessage &msg) {
  if (msg.isFloat(0))
    vco.setF0(msg.getFloat(0));
}

/**
 * /synth/vco/lfo_mod "f" <amount>
 * 
 * Scalar dictating how much the LFO modulates the VCO frequency.
 */
void synth_handle_vco_lfo_mod(OSCMessage &msg) {
  if (msg.isFloat(0))
    vco.setF0ModAmp(msg.getFloat(0));
}

/**
 * /synth/vca/lfo_mod "f" <amount>
 * 
 * Scalar dictating how much the LFO modulates the VCA amplitude.
 */
void synth_handle_vca_lfo_mod(OSCMessage &msg) {
  if (msg.isFloat(0))
    synth_vca_lfo_mod = msg.getFloat(0);
}

/* --------------------------------- */
/* === Audio feedback parameters === */
/* --------------------------------- */

/**
 * /fb/gain "f" <gain>
 * 
 * Audio input gain.
 */
void fb_handle_gain(OSCMessage &msg) {
  if (msg.isFloat(0))
    fb_gain = msg.getFloat(0);
}

/**
 * /fb/phase "f" <samples>
 * 
 * Audio input "phase" delay in samples. Dictates the number of samples that the feedback 
 * audio output lags the input.
 */
void fb_handle_phase(OSCMessage &msg) {
  if (msg.isFloat(0))
    sample_delay = msg.getFloat(0);
}

/**
 * /fb/vca/lfo_mod "f" <amount>
 * 
 * Dictates how much the LFO modulates the feedback audio output amplitude. 
 */
void fb_handle_vca_lfo_mod(OSCMessage &msg) {
  if (msg.isFloat(0))
    fb_vca_lfo_mod = msg.getFloat(0);
}

/**
 * /fb/vca/env_mod "f" <amount>
 * 
 * Dictates how much the envelope generator modulates the feedback audio output amplitude.
 */
void fb_handle_vca_env_mod(OSCMessage &msg) {
  if (msg.isFloat(0))
    fb_vca_env_mod = msg.getFloat(0);
}

/**
 * /mixer/synth_feedback_mix "f" <mix>
 * 
 * Dictates the mix of synth and audio feedback in the output signal. Required range
 * [0, 1], where 0 is all synth, and 1 is all audio feedback. 
 */
void mixer_handle_synth_feedback_mix(OSCMessage &msg) {
  if (msg.isFloat(0))
    fb_mix = msg.getFloat(0);
}

/* === Utility === */
void slip_send(OSCMessage &msg) {
  SLIPSerial.beginPacket();
  msg.send(SLIPSerial);
  SLIPSerial.endPacket();
}

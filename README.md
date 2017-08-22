# ActuatedDrums

System for electromagnetic actuation of acoustic drums controlled via Open Sound Control (OSC) over Wi-Fi.

## Hardware

A single drum module system consists of two custom boards, including a power amplifier/supply board to drive the electromagnet, and a preamplifier and signal processor board. Audio DSP is done on a Teensy 3.6, and an ESP8266-01 connects to a dedicated local "Drumhenge" network to receive/send OSC messages relayed to/from the Teensy. Board layouts, and links for parts sourcing are located in the "Drum Module" directory.

The power amplifier is the same used in the Magnetic Resonator Piano (modified for through-hole construction), whose design is detailed in [McPherson 2012](http://vhosts.eecs.umich.edu/nime2012//Proceedings/papers/117_Final_Manuscript.pdf). The module is powered by a 15V, 4A DC adapter and includes a switching power supply section to generate the negative supply rail for the power amplifier, and the low-voltage supply for the preamplifier/processor board.

Note: the power amplifier should only be used in single-ended mode. Though the switch allows bypassing the level shifting amplifiers to run in bipolar mode (for driving magnetized objects), the LT1054 used in the current version can only supply 100mA on the negative 15V rail, which is insufficient for bipolar operation. 

The preamplifier consists of amplifiers and anti-aliasing filters before and after the Teensy, as well as three potentiometers controlling CVs that are mappable to synth or calibration parameters. The preamplifier board also has a 15V LED driver for controlling an RGB LED bulb mounted inside the drum. 

## Software

Drum modules are configured using a central controller application DrumNetworkController, and a local network (SSID: Drumhenge, Pass: ahengeofdrums). Recommend configuring the Drumhenge network router to reserve IP addresses for each module, so that modules can be numbered and order can be preserved for easy setup of propagation mode. 

### DrumNode

Main signal processing and control code for the Teensy 3.6. DSP is currently performed sample-by-sample. See the main DrumNode.ino file for the most up-to-date ADC/DAC resolution and sample rate parameters, potentiometer mappings, and OSC message list. 

### OSCHandler

Configures the ESP8266-01 to send and receive OSC messages via UDP, and relay them to the Teensy 3.6 via SLIP Serial messaging. Devices are currently hard-coded to connect to the Drumhenge network. Each device is assigned a local IP address by the network, and opens UDP port 7770 to receive OSC messages specifically for this module. Each device also opens a multicast port at IP address 239.0.0.1 for receiving OSC messages sent to every device on the network.

### DrumNetworkController

Native OS X application for configuration of any number of drum modules. Sends an OSC message to the multicast port to request each module's local IP address. The application can then set synthesis parameters for individual modules or all modules, configure propagation mode by assigning modules as 'listeners' for other modules, and translate incoming MIDI note and CC messages to OSC for use of the drum network as a multi-voice synthesizer. 

## Issues and To-Do List

### Hardware
* Power supply re-design
	* Though the power amplifier can be switched to run in single-ended or bipolar mode, the single LT1054 can only supply up to 100mA of current for the negative rail, which is insufficient for bipolar operation (used for driving magnetized objects).
	* Re-design using paralleled LT1054s for higher output current, or an alternative implementation of a high-current negative rail. 
	* Add transistor indicated in Datasheet's Figure 30 to prevent Vout being pulled above GND during power-up, since the load is connected between Vcc and Vout
* Power amplifier re-design
	* Add short circuit protection to prevent fried magnets from damaging amp chips
* Preamplifier re-design
	* Remove pots from op amp feedback loop and investigate effect on succeptibility to radio noise.
	* Add amplifier and anti-aliasing filter for second output channel
		* Also expand inter-board connector from 5 to 7 pins for automatic routing of second channel to power amplifier and channel 2 mute/standby control. 
	* Capacitive sensing using touchRead() becomes unreliable when the power amplifier starts drawing current from the power supply and returning ground currents that effect the sensitivity of the Teensy's capacitive sensing.
		* Need separate external capacitive proximity sensing module based on CD40106 hex schmitt trigger relaxation oscillator with drum hoop serving as the timing capacitor. Divide frequency using CD4040 binary counter, and configure ESP8266 as a frequency counter using pin interrupts. Send OSC in response to thresholded changes in frequency. Configure parameter mapping using DrumNetworkController.
		


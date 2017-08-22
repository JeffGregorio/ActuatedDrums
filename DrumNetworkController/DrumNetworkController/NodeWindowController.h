//
//  NodeWindowController.h
//  DrumNetworkController
//
//  Created by Jeff Gregorio on 5/31/17.
//  Copyright © 2017 Jeff Gregorio. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include "lo/lo.h"
#include <vector>

@interface NodeWindowController : NSWindowController {
    lo_address node_address;
    lo_address multicast_address;
    lo_address dest_address;
    std::vector<lo_address> node_addresses;
}

@property IBOutlet NSTextField *addressTextField;
@property IBOutlet NSButton *multicastButton;
@property IBOutlet NSPopUpButton *thereminDestButton;
@property IBOutlet NSTextField *thereminDestPortField;
@property IBOutlet NSTextField *thereminPathField;
@property IBOutlet NSTextField *thereminValueMinField;
@property IBOutlet NSTextField *thereminValueMaxField;

- (id)initWithWindowNibName:(NSString *)windowNibName
                nodeAddress:(lo_address)n_addr
           multicastAddress:(lo_address)m_addr;

- (IBAction)multicastButtonChanged:(id)sender;
- (IBAction)mute_changed:(NSButton *)sender;

#pragma mark - Theremin
- (IBAction)theremin_update_mapping:(NSButton *)sender;
- (IBAction)theremin_mode_changed:(NSSegmentedControl *)sender;

#pragma mark - Modulation Sources
// LFO
- (IBAction)mod_lfo_wave_shape_changed:(NSSegmentedControl *)sender;
- (IBAction)mod_lfo_rate_changed:(NSSlider *)sender;
- (IBAction)mod_lfo_env_mod_changed:(NSSlider *)sender;

// ENV
- (IBAction)mod_env_atk_changed:(NSSlider *)sender;
- (IBAction)mod_env_sus_changed:(NSSlider *)sender;
- (IBAction)mod_env_rel_changed:(NSSlider *)sender;
- (IBAction)mod_env_do_sus_changed:(NSButton *)sender;
- (IBAction)mod_env_follower_gate_changed:(NSButton *)sender;
- (IBAction)mod_env_gate_on:(NSButton *)sender;
- (IBAction)mod_env_gate_off:(NSButton *)sender;

#pragma mark - Synth
// VCO
- (IBAction)synth_vco_wave_shape_changed:(NSSegmentedControl *)sender;
- (IBAction)synth_vco_freq_changed:(NSSlider *)sender;
- (IBAction)synth_vco_lfo_mod_changed:(NSSlider *)sender;

// VCA
- (IBAction)synth_vca_lfo_mod_changed:(NSSlider *)sender;

#pragma mark - Audio input
// Audio
- (IBAction)fb_gain_changed:(NSSlider *)sender;
- (IBAction)fb_phase_changed:(NSSlider *)sender;

// VCA
- (IBAction)fb_vca_lfo_mod_changed:(NSSlider *)sender;
- (IBAction)fb_vca_env_mod_changed:(NSSlider *)sender;

#pragma mark - Mixer
- (IBAction)mix_changed:(NSSlider *)sender;

#pragma mark - Propagation
- (IBAction)propagate_enable_changed:(NSButton *)sender;
- (IBAction)propagate_kill_pressed:(NSButton *)sender;
- (IBAction)propagate_decay_changed:(NSSlider *)sender;

@end

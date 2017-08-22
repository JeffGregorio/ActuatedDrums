//
//  NodeWindowController.m
//  DrumNetworkController
//
//  Created by Jeff Gregorio on 5/31/17.
//  Copyright © 2017 Jeff Gregorio. All rights reserved.
//

#import "NodeWindowController.h"
#import "AppDelegate.h"

@interface NodeWindowController ()

@end

@implementation NodeWindowController
@synthesize addressTextField;
@synthesize multicastButton;

- (id)initWithWindowNibName:(NSString *)windowNibName
                nodeAddress:(lo_address)n_addr
           multicastAddress:(lo_address)m_addr {
    
    node_address = dest_address = n_addr;
    multicast_address = m_addr;
    self = [super initWithWindowNibName:windowNibName];
    if (self)
        return self;
    else return nil;
}

- (void)windowDidLoad {
    [super windowDidLoad];
    const char *address = lo_address_get_hostname(node_address);
    int bytes[4];
    sscanf(address, "%d.%d.%d.%d", bytes, bytes+1, bytes+2, bytes+3);
    [addressTextField setStringValue:
     [NSString stringWithFormat:@"%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]]];
    
    [self setUpThereminMapping];
}

- (void)setUpThereminMapping {
    
    // Destination IP addresses
    AppDelegate *delegate = (AppDelegate *)[NSApp delegate];
    node_addresses = [delegate get_node_addresses];
    for (int i = 0; i < node_addresses.size(); i++) {
        [_thereminDestButton addItemWithTitle:
         [NSString stringWithUTF8String:lo_address_get_hostname(node_addresses[i])]];
    }
}

- (IBAction)multicastButtonChanged:(id)sender {
    if ([multicastButton state] == NSOnState)
        dest_address = multicast_address;
    else
        dest_address = node_address;
}

- (IBAction)mute_changed:(NSButton *)sender {
    lo_send(dest_address, "/mute", "i", sender.state == NSOnState);
}

#pragma mark - Theremin
- (IBAction)theremin_update_mapping:(NSButton *)sender {
    NSString *dest = [_thereminDestButton titleOfSelectedItem];
    NSString *param = [_thereminPathField stringValue];
    int port = [_thereminDestPortField intValue];
    double min = [_thereminValueMinField doubleValue];
    double max = [_thereminValueMaxField doubleValue];
    NSLog(@"%s: /theremin/map <%@><%d><%@><%f><%f>", __func__, dest, port, param, min, max);
    lo_send(dest_address, "/theremin/map", "sisff",
            dest.UTF8String, port, param.UTF8String, (float)min, (float)max);
}

- (IBAction)theremin_mode_changed:(NSSegmentedControl *)sender {
    switch([sender selectedSegment]) {
        case 0:
            lo_send(dest_address, "/theremin/enable", "i", 0);
            lo_send(dest_address, "/theremin/calibrate", "i", 0);
            break;
        case 1:
            lo_send(dest_address, "/theremin/enable", "i", 0);
            lo_send(dest_address, "/theremin/calibrate", "i", 1);
            break;
        case 2:
            lo_send(dest_address, "/theremin/enable", "i", 1);
            lo_send(dest_address, "/theremin/calibrate", "i", 0);
            break;
    }
}

#pragma mark - Modulation Sources
- (IBAction)mod_lfo_wave_shape_changed:(NSSegmentedControl *)sender {
    NSString *shape;
    switch ([sender selectedSegment]) {
        case 0:
            shape = @"sine";
            break;
        case 1:
            shape = @"square";
            break;
        case 2:
            shape = @"saw";
            break;
    }
    lo_send(dest_address, "/mod/lfo/wave_shape", "s", [shape UTF8String]);
}

- (IBAction)mod_lfo_rate_changed:(NSSlider *)sender {
    lo_send(dest_address, "/mod/lfo/rate", "f", sender.floatValue);
}

- (IBAction)mod_lfo_env_mod_changed:(NSSlider *)sender {
    lo_send(dest_address, "/mod/lfo/env_mod", "f", sender.floatValue);
}

- (IBAction)mod_env_atk_changed:(NSSlider *)sender {
    lo_send(dest_address, "/mod/egen/atk_time", "f", sender.floatValue);
}

- (IBAction)mod_env_sus_changed:(NSSlider *)sender {
    lo_send(dest_address, "/mod/egen/sus_level", "f", sender.floatValue);
}

- (IBAction)mod_env_rel_changed:(NSSlider *)sender {
    lo_send(dest_address, "/mod/egen/rel_time", "f", sender.floatValue);
}

- (IBAction)mod_env_do_sus_changed:(NSButton *)sender {
    lo_send(dest_address, "/mod/egen/do_sus", "i", sender.state == NSOnState ? 1 : 0);
}

- (IBAction)mod_env_follower_gate_changed:(NSButton *)sender {
    lo_send(dest_address, "/mod/egen/follower_gate", "i", sender.state == NSOnState ? 1 : 0);
}

- (IBAction)mod_env_gate_on:(NSButton *)sender {
    lo_send(dest_address, "/mod/egen/gate", "i", 1);
}

- (IBAction)mod_env_gate_off:(NSButton *)sender {
    lo_send(dest_address, "/mod/egen/gate", "i", 0);
}

#pragma mark - Synth
- (IBAction)synth_vco_wave_shape_changed:(NSSegmentedControl *)sender {
    NSString *shape;
    switch ([sender selectedSegment]) {
        case 0:
            shape = @"sine";
            break;
        case 1:
            shape = @"square";
            break;
        case 2:
            shape = @"saw";
            break;
    }
    lo_send(dest_address, "/synth/vco/wave_shape", "s", [shape UTF8String]);
}

- (IBAction)synth_vco_freq_changed:(NSSlider *)sender {
    lo_send(dest_address, "/synth/vco/freq_", "f", sender.floatValue);
}

- (IBAction)synth_vco_lfo_mod_changed:(NSSlider *)sender {
    lo_send(dest_address, "/synth/vco/lfo_mod", "f", sender.floatValue);
}

- (IBAction)synth_vca_lfo_mod_changed:(NSSlider *)sender {
    lo_send(dest_address, "/synth/vca/lfo_mod", "f", sender.floatValue);
}

#pragma mark - Audio input
- (IBAction)fb_gain_changed:(NSSlider *)sender {
    lo_send(dest_address, "/fb/gain", "f", sender.floatValue);
}

- (IBAction)fb_phase_changed:(NSSlider *)sender {
    lo_send(dest_address, "/fb/phase", "f", sender.floatValue);
}

- (IBAction)fb_vca_lfo_mod_changed:(NSSlider *)sender {
    lo_send(dest_address, "/fb/vca/lfo_mod_", "f", sender.floatValue);
}

- (IBAction)fb_vca_env_mod_changed:(NSSlider *)sender {
    lo_send(dest_address, "/fb/vca/env_mod_", "f", sender.floatValue);
}

#pragma mark - Mixer
- (IBAction)mix_changed:(NSSlider *)sender {
    lo_send(dest_address, "/mixer/synth_feedback_mix", "f", sender.floatValue);
}

#pragma mark - Propagation
- (IBAction)propagate_enable_changed:(NSButton *)sender {
    lo_send(dest_address, "/propagate/enable", "i", sender.state == NSOnState ? 1 : 0);
}

- (IBAction)propagate_kill_pressed:(NSButton *)sender {
    lo_send(dest_address, "/propagate/kill", NULL);
}

- (IBAction)propagate_decay_changed:(NSSlider *)sender {
    lo_send(dest_address, "/propagate/decay", "f", sender.floatValue);
}

@end

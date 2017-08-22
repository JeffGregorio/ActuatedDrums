//
//  AppDelegate.h
//  DrumNetworkController
//
//  Created by Jeff Gregorio on 2/14/17.
//  Copyright © 2017 Jeff Gregorio. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "NodeWindowController.h"
#include "lo/lo.h"
#include <stdlib.h>
#include <vector>
#include <map>
#include "RtMidi.h"

#define kMulticast_Address "239.0.0.1"
#define kMulticast_Port "7771"
#define kLocal_Port "7770"
#define kMIDIDeviceQueryIntervalSeconds (5.0)
#define kMIDICCTimerInterval (0.01)

@interface AppDelegate : NSObject <NSApplicationDelegate, NSTableViewDelegate, NSTableViewDataSource> {
    
    RtMidiIn *midiIn;
    NSTimer *midiDeviceQueryTimer;
    int numMidiDevices;
    std::map<int, int> noteAddressMap;
    std::vector<int> node_idx_randperm;
    std::vector<int> node_idx_distributed;
    std::vector<int> node_idx_hemispheric;
    int node_idx;
    
    // MIDI CC
    NSMutableArray *cc_paths;       // CC mapping output OSC paths
    int cc_nums[8];                 // CC mapping source num
    float cc_min[8];                // CC output range min
    float cc_max[8];                // " " " max
    float cc_scaled_current[8];     // CC output (scaled)
    bool cc_scaled_changed[8];      // CC output update flag
    NSTimer *midi_cc_timer;         // CC output update timer
    int cc_update_idx;              // CC output to update
    
    lo_server_thread osc_server_thread;
    lo_address multicast_address;
    lo_address local_address;
    
    std::vector<lo_address> node_addresses;
    
    // All on allocation
    int num_notes_previous;
    std::vector<int> current_notes;
    std::vector<int> node_note_nums;
    std::vector<int> node_note_nums_previous;
    
    NSMutableArray *node_windows;
    
    NSPopUpButton *midi_cc_selection;
}

- (void)query_midi_devices;
- (void)midi_note_handler:(int)num velocity:(int)vel;
- (void)midi_cc_handler:(int)num value:(int)val;

- (void)add_node:(const char *)address;

@property IBOutlet NSTableView *nodeTableView;

@property (weak) IBOutlet NSPopUpButton *midiDevicePopUpButton;
@property (weak) IBOutlet NSSegmentedControl *midiNoteAllocationModeControl;
@property (weak) IBOutlet NSPopUpButton *midi_cc1_src;
@property (weak) IBOutlet NSPopUpButton *midi_cc2_src;
@property (weak) IBOutlet NSPopUpButton *midi_cc3_src;
@property (weak) IBOutlet NSPopUpButton *midi_cc4_src;
@property (weak) IBOutlet NSPopUpButton *midi_cc5_src;
@property (weak) IBOutlet NSPopUpButton *midi_cc6_src;
@property (weak) IBOutlet NSPopUpButton *midi_cc7_src;
@property (weak) IBOutlet NSPopUpButton *midi_cc8_src;
@property (weak) IBOutlet NSTextField *midi_cc1_dest;
@property (weak) IBOutlet NSTextField *midi_cc2_dest;
@property (weak) IBOutlet NSTextField *midi_cc3_dest;
@property (weak) IBOutlet NSTextField *midi_cc4_dest;
@property (weak) IBOutlet NSTextField *midi_cc5_dest;
@property (weak) IBOutlet NSTextField *midi_cc6_dest;
@property (weak) IBOutlet NSTextField *midi_cc7_dest;
@property (weak) IBOutlet NSTextField *midi_cc8_dest;
@property (weak) IBOutlet NSTextField *midi_cc1_min;
@property (weak) IBOutlet NSTextField *midi_cc2_min;
@property (weak) IBOutlet NSTextField *midi_cc3_min;
@property (weak) IBOutlet NSTextField *midi_cc4_min;
@property (weak) IBOutlet NSTextField *midi_cc5_min;
@property (weak) IBOutlet NSTextField *midi_cc6_min;
@property (weak) IBOutlet NSTextField *midi_cc7_min;
@property (weak) IBOutlet NSTextField *midi_cc8_min;
@property (weak) IBOutlet NSTextField *midi_cc1_max;
@property (weak) IBOutlet NSTextField *midi_cc2_max;
@property (weak) IBOutlet NSTextField *midi_cc3_max;
@property (weak) IBOutlet NSTextField *midi_cc4_max;
@property (weak) IBOutlet NSTextField *midi_cc5_max;
@property (weak) IBOutlet NSTextField *midi_cc6_max;
@property (weak) IBOutlet NSTextField *midi_cc7_max;
@property (weak) IBOutlet NSTextField *midi_cc8_max;

@property (weak) IBOutlet NSButton *propagation_enabled_check;
@property (weak) IBOutlet NSButton *follower_gate_enabled_check;



#pragma mark - MIDI
- (IBAction)midi_device_selected:(NSPopUpButton *)sender;
- (IBAction)midi_cc_selected:(NSPopUpButton *)sender;
- (IBAction)midi_cc_dest_changed:(NSTextField *)sender;
- (IBAction)midi_cc_min_changed:(NSTextField *)sender;
- (IBAction)midi_cc_max_changed:(NSTextField *)sender;

#pragma mark - Network Config
- (IBAction)multi_send_get_ip:(id)sender;
- (IBAction)multi_send_remove_listeners:(id)sender;
- (std::vector<lo_address>)get_node_addresses;

#pragma mark - Propagation
- (IBAction)propagation_enabled_changed:(NSButton *)sender;
- (IBAction)follower_gate_changed:(NSButton *)sender;
- (IBAction)propagation_decay_changed:(NSSlider *)sender;
- (IBAction)propagation_kill_pressed:(NSButton *)sender;
- (IBAction)propagation_direction_set_cw:(NSButton *)sender;
- (IBAction)propagation_direction_set_ccw:(NSButton *)sender;
- (IBAction)propagation_direction_set_bidir:(NSButton *)sender;

@end


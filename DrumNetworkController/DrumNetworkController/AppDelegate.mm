//
//  AppDelegate.m
//  DrumNetworkController
//
//  Created by Jeff Gregorio on 2/14/17.
//  Copyright © 2017 Jeff Gregorio. All rights reserved.
//

#import "AppDelegate.h"

void error(int num, const char *msg, const char *path) {
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
    fflush(stdout);
}

int ip_handler(const char *path, const char *types, lo_arg ** argv, int argc, void *data, void *user_data) {
    
    AppDelegate *delegate = (__bridge AppDelegate *)user_data;
    
    char address[16];
    sprintf(address, "%d.%d.%d.%d", argv[0]->i, argv[1]->i, argv[2]->i, argv[3]->i);
    [delegate add_node:address];
    
    return 1;
}

int generic_handler(const char *path, const char *types, lo_arg ** argv, int argc, void *data, void *user_data) {
    
    printf("\npath: <%s>\n", path);
    for (int i = 0; i < argc; i++) {
        printf("- arg %d '%c' ", i, types[i]);
        lo_arg_pp((lo_type)types[i], argv[i]);
        printf("\n");
    }
    printf("\n");
    fflush(stdout);
    
    return 1;
}

void midiCallback(double deltatime, std::vector< unsigned char > *message, void *userData) {
    
    unsigned int nBytes = (unsigned int)message->size();
    
    // Only interested in note on/off messages
//    if (nBytes != 3)
//        return;
    
    AppDelegate *delegate = (__bridge AppDelegate*)userData;
    
    int cmd = message->at(0);
    int status = (cmd & 0xF0);
    
    switch (status) {
        case 0x90:      // Note on
            [delegate midi_note_handler:message->at(1) velocity:message->at(2)];
            break;
        case 0x80:      // Note off
            [delegate midi_note_handler:message->at(1) velocity:0];
            break;
        case 0xB0:      // Control change
            [delegate midi_cc_handler:message->at(1) value:message->at(2)];
            break;
        default:
            printf("MIDI: ");
            for (int i = 0; i < nBytes; i++)
                printf("%d\t", message->at(i));
            printf("\n");
            break;
    }
}


@interface AppDelegate ()

@property (weak) IBOutlet NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    [self setup];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    lo_server_thread_free(osc_server_thread);
}

- (void)setup {
    
    // MIDI Devices
    midiIn = new RtMidiIn();
    numMidiDevices = 0;
    [self query_midi_devices];
    
    // Set up timer to query MIDI devices periodically
    midiDeviceQueryTimer = [NSTimer scheduledTimerWithTimeInterval:kMIDIDeviceQueryIntervalSeconds
                                                            target:self
                                                          selector:@selector(query_midi_devices)
                                                          userInfo:nil
                                                           repeats:YES];
    // MIDI Notes
    node_idx = 0;
    num_notes_previous = 0;
    
    // MIDI CC
    [self setUpMIDICCMapping];
    
    for (int i = 0; i < 127; i++)
        noteAddressMap[i] = -1;
    midiIn->openVirtualPort("Drumhenge");
    
    // OSC server
    osc_server_thread = lo_server_thread_new(kMulticast_Port, error);
    local_address = lo_address_new("10.0.1.2", kLocal_Port);
    lo_server_thread_add_method(osc_server_thread,
                                NULL, NULL,
                                generic_handler,
                                (__bridge void *)self);
    lo_server_thread_add_method(osc_server_thread,
                                "/ip", "iiii",
                                ip_handler,
                                (__bridge void *)self);
    lo_server_thread_start(osc_server_thread);
    
    // OSC client
    multicast_address = lo_address_new(kMulticast_Address, kMulticast_Port);
    
    // TableView
    [_nodeTableView setDelegate:self];
    [_nodeTableView setDataSource:self];
    
    // Node Parameter Windows
    node_windows = [[NSMutableArray alloc] init];
    
    for (NSTableColumn *tableColumn in _nodeTableView.tableColumns ) {
        NSSortDescriptor *sortDescriptor = [NSSortDescriptor sortDescriptorWithKey:tableColumn.identifier ascending:YES selector:@selector(compare:)];
        [tableColumn setSortDescriptorPrototype:sortDescriptor];
    }
}

#pragma mark - MIDI Devices
- (void)query_midi_devices {
    
    // Only re-populate the pop up button if the number of devices has changed
    int nPorts = midiIn->getPortCount();
    if (nPorts == numMidiDevices)
        return;
    
    NSLog(@"%s: %d available MIDI devices", __func__, nPorts);
    
    if (nPorts > numMidiDevices) {      // Add newly available devices
        NSString *portName;
        for (int i = 0; i < nPorts; i++) {
            bool found = false;
            portName = [NSString stringWithUTF8String:midiIn->getPortName(i).c_str()];
            for (int j = 0; j < numMidiDevices && !found; j++) {
                found = [_midiDevicePopUpButton itemWithTitle:portName] != nil;
            }
            if (!found) {
                [_midiDevicePopUpButton addItemWithTitle:portName];
                [[_midiDevicePopUpButton lastItem] setTag:i];
                NSLog(@"%s: added device %d: %s", __func__, i, midiIn->getPortName(i).c_str());
            }
        }
    }
    else if (numMidiDevices > nPorts) {             // Remove devices that became unavailable
        for (int j = 0; j < numMidiDevices; j++) {
            bool found = false;
            NSString *portName;
            for (int i = 0; i < nPorts && !found; i++) {
                portName = [NSString stringWithUTF8String:midiIn->getPortName(i).c_str()];
                found = [[_midiDevicePopUpButton itemTitleAtIndex:j] isEqualToString:portName];
            }
            if (!found) {
                NSLog(@"%s: removed device %d: %@", __func__, j, [_midiDevicePopUpButton itemTitleAtIndex:j]);
                [_midiDevicePopUpButton removeItemAtIndex:j];
            }
        }
    }
    numMidiDevices = nPorts;
}

#pragma mark - MIDI Note Handling/Allocation
- (void)midi_note_handler:(int)num velocity:(int)vel {
    
    if (node_addresses.size() == 0)
        return;
    
    lo_address dest;
    int dest_idx;
    
    if ([_midiNoteAllocationModeControl selectedSegment] == 3)
        [self allocate_all_on:num velocity:vel];
    
    else {  // All other allocation modes
        
        // Note ON
        if (vel != 0) {
            dest_idx = [self allocate_node];
            dest = node_addresses[dest_idx];
            noteAddressMap[num] = dest_idx;
            NSLog(@"%s: -- NOTE ON: assigning to node %s", __func__, lo_address_get_hostname(dest));
            lo_send(dest, "/note", "ii", num, vel);
        }
        // Note OFF
        else {
            dest = node_addresses[noteAddressMap[num]];
            if (dest) {
                NSLog(@"%s: -- NOTE OFF: turning off node %s", __func__, lo_address_get_hostname(dest));
                lo_send(dest, "/note", "ii", num, vel);
                noteAddressMap[num] = -1;
            }
        }
    }
}

- (void)allocate_all_on:(int)num velocity:(int)vel {

    int num_nodes = (int)node_addresses.size();
    int num_notes;
    int nodes_per_note;
    int remainder;
    
    // Add or remove note number from current notes vector
    if (vel != 0)
        current_notes.push_back(num);
    else {
        auto it = std::find(current_notes.begin(), current_notes.end(), num);
        if(it != current_notes.end())
            current_notes.erase(it);
    }
    
    // Allocation parameters
    num_notes = (int)current_notes.size();
    nodes_per_note = floor(num_nodes / (float)num_notes);
    remainder = num_nodes - nodes_per_note * num_notes;
    
    // Store previous allocations vector
    node_note_nums_previous = node_note_nums;
    
    // Update allocations vector
    if (num_notes == 0) {
        for (int i = 0; i < num_nodes; i++)
            node_note_nums[i] = -1;
    }
    else if (num_notes > num_notes_previous) {
        int n = 0;
        for (int i = 0; i < num_notes; i++) {
            for (int j = 0; j < nodes_per_note; j++)
                node_note_nums[n++] = current_notes[i];
            if (remainder > 0) {
                node_note_nums[n++] = current_notes[i];
                remainder--;
            }
        }
    }
    else {
        for (int i = 0; i < num_nodes; i++) {
            if (node_note_nums[i] == num)
                node_note_nums[i] = -1;
        }
    }
    
    // Send messages to nodes needing update
    for (int i = 0; i < node_note_nums.size(); i++) {
        if (node_note_nums[i] == -1)                                // Note off
            lo_send(node_addresses[i], "/note", "ii", num, 0);
        else if (node_note_nums_previous[i] == -1)                  // Note on
            lo_send(node_addresses[i], "/note", "ii", num, vel);
        else if (node_note_nums_previous[i] != node_note_nums[i])   // Pitch change
            lo_send(node_addresses[i], "/synth/vco/freq_", "f", powf(2.0, (num - 69) / 12.0) * 440.0);
    }
    num_notes_previous = num_notes;
}

- (int)allocate_node {
    int idx;
    switch ([_midiNoteAllocationModeControl selectedSegment]) {
            
        case 1:         // Random
            idx = node_idx_randperm[node_idx];
            break;
        case 2:         // Distributed
            idx = node_idx_distributed[node_idx];
            break;
        case 4:         // Hemispheric
            idx = node_idx_hemispheric[node_idx];
            break;
        case 0:         // Sequential
        default:
            idx = node_idx;
            break;
    }
    node_idx++;
    if (node_idx == node_addresses.size()) {
        node_idx = 0;
        node_idx_randperm = [self arrange_randperm:(int)node_addresses.size()];
    }
    return idx;
}

- (std::vector<int>)arrange_randperm:(int)num_nodes {
    std::vector<int> vec;
    for (int i = 0; i < num_nodes; i++) vec.push_back(i);
    for (int i = 0; i < num_nodes; i++) {
        int j, t;
        j = rand() % (num_nodes-i) + i;
        t = vec[j];
        vec[j] = vec[i];
        vec[i] = t;
    }
    return vec;
}

- (std::vector<int>)arrange_distributed:(int)num_nodes {
    std::vector<int> vec;
    
    return vec;
}

- (std::vector<int>)arrange_hemispheric:(int)num_nodes {
    std::vector<int> vec;
    int i, j, k;
    for (i = 0, j = 0, k = num_nodes-1; i < num_nodes; i++) {
        if (i % 2 == 0)
            vec.push_back(j++);
        else
            vec.push_back(k--);
    }
    return vec;
}

#pragma mark - MIDI CC Handling/Mapping
- (void)setUpMIDICCMapping {
    
    // Add possible CC nums to pop up buttons
    for (int i = 0; i < 127; i++) {
        [_midi_cc1_src addItemWithTitle:[NSString stringWithFormat:@"%d", i]];
        [[_midi_cc1_src lastItem] setTag:i];
        [_midi_cc2_src addItemWithTitle:[NSString stringWithFormat:@"%d", i]];
        [[_midi_cc2_src lastItem] setTag:i];
        [_midi_cc3_src addItemWithTitle:[NSString stringWithFormat:@"%d", i]];
        [[_midi_cc3_src lastItem] setTag:i];
        [_midi_cc4_src addItemWithTitle:[NSString stringWithFormat:@"%d", i]];
        [[_midi_cc4_src lastItem] setTag:i];
        [_midi_cc5_src addItemWithTitle:[NSString stringWithFormat:@"%d", i]];
        [[_midi_cc5_src lastItem] setTag:i];
        [_midi_cc6_src addItemWithTitle:[NSString stringWithFormat:@"%d", i]];
        [[_midi_cc6_src lastItem] setTag:i];
        [_midi_cc7_src addItemWithTitle:[NSString stringWithFormat:@"%d", i]];
        [[_midi_cc7_src lastItem] setTag:i];
        [_midi_cc8_src addItemWithTitle:[NSString stringWithFormat:@"%d", i]];
        [[_midi_cc8_src lastItem] setTag:i];
    }
    
    // Set up notifications for automatically selecting CC num when menu is open and CC is received
    midi_cc_selection = nil;
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(midi_cc1_selected)
                                                 name:NSPopUpButtonWillPopUpNotification
                                               object:_midi_cc1_src];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(midi_cc2_selected)
                                                 name:NSPopUpButtonWillPopUpNotification
                                               object:_midi_cc2_src];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(midi_cc3_selected)
                                                 name:NSPopUpButtonWillPopUpNotification
                                               object:_midi_cc3_src];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(midi_cc4_selected)
                                                 name:NSPopUpButtonWillPopUpNotification
                                               object:_midi_cc4_src];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(midi_cc5_selected)
                                                 name:NSPopUpButtonWillPopUpNotification
                                               object:_midi_cc5_src];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(midi_cc6_selected)
                                                 name:NSPopUpButtonWillPopUpNotification
                                               object:_midi_cc6_src];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(midi_cc7_selected)
                                                 name:NSPopUpButtonWillPopUpNotification
                                               object:_midi_cc7_src];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(midi_cc8_selected)
                                                 name:NSPopUpButtonWillPopUpNotification
                                               object:_midi_cc8_src];
    
    cc_paths = [[NSMutableArray alloc] init];
    for (int i = 0; i < 8; i++)
        [cc_paths insertObject:@"" atIndex:i];
    
    // Set default mappings
    dispatch_async(dispatch_get_main_queue(),^{
        [_midi_cc1_src selectItemWithTitle:[NSString stringWithFormat:@"%d", 20]];
        [_midi_cc1_dest setStringValue:@"/mod/egen/atk_time"];
        [self midi_cc_selected:_midi_cc1_src];
        [self midi_cc_dest_changed:_midi_cc1_dest];
        [self midi_cc_min_changed:_midi_cc1_min];
        [self midi_cc_max_changed:_midi_cc1_max];
        [_midi_cc2_src selectItemWithTitle:[NSString stringWithFormat:@"%d", 21]];
        [_midi_cc2_dest setStringValue:@"/mod/egen/rel_time"];
        [self midi_cc_selected:_midi_cc2_src];
        [self midi_cc_dest_changed:_midi_cc2_dest];
        [self midi_cc_min_changed:_midi_cc2_min];
        [self midi_cc_max_changed:_midi_cc2_max];
        [_midi_cc3_src selectItemWithTitle:[NSString stringWithFormat:@"%d", 22]];
        [_midi_cc3_dest setStringValue:@"/mod/lfo/rate"];
        [self midi_cc_selected:_midi_cc3_src];
        [self midi_cc_dest_changed:_midi_cc3_dest];
        [self midi_cc_min_changed:_midi_cc3_min];
        [self midi_cc_max_changed:_midi_cc3_max];
        [_midi_cc4_src selectItemWithTitle:[NSString stringWithFormat:@"%d", 23]];
        [_midi_cc4_dest setStringValue:@"/synth/vca/lfo_mod"];
        [self midi_cc_selected:_midi_cc4_src];
        [self midi_cc_dest_changed:_midi_cc4_dest];
        [self midi_cc_min_changed:_midi_cc4_min];
        [self midi_cc_max_changed:_midi_cc4_max];
        [_midi_cc5_src selectItemWithTitle:[NSString stringWithFormat:@"%d", 24]];
        [_midi_cc5_dest setStringValue:@"/synth/vco/lfo_mod"];
        [self midi_cc_selected:_midi_cc5_src];
        [self midi_cc_dest_changed:_midi_cc5_dest];
        [self midi_cc_min_changed:_midi_cc5_min];
        [self midi_cc_max_changed:_midi_cc5_max];
        [_midi_cc6_src selectItemWithTitle:[NSString stringWithFormat:@"%d", 25]];
        [_midi_cc6_dest setStringValue:@"/mod/lfo/env_mod"];
        [self midi_cc_selected:_midi_cc6_src];
        [self midi_cc_dest_changed:_midi_cc6_dest];
        [self midi_cc_min_changed:_midi_cc6_min];
        [self midi_cc_max_changed:_midi_cc6_max];
        [_midi_cc7_src selectItemWithTitle:[NSString stringWithFormat:@"%d", 26]];
        [_midi_cc7_dest setStringValue:@"/mixer/synth_feedback_mix"];
        [self midi_cc_selected:_midi_cc7_src];
        [self midi_cc_dest_changed:_midi_cc7_dest];
        [self midi_cc_min_changed:_midi_cc7_min];
        [self midi_cc_max_changed:_midi_cc7_max];
//        [_midi_cc8_src selectItemWithTitle:[NSString stringWithFormat:@"%d", 9]];
//        [self midi_cc_selected:_midi_cc8_src];
//        [self midi_cc_dest_changed:_midi_cc8_dest];
//        [self midi_cc_min_changed:_midi_cc8_min];
//        [self midi_cc_max_changed:_midi_cc8_max];
    });
}

- (void)midi_cc1_selected {
    NSLog(@"%s", __func__);
    midi_cc_selection = _midi_cc1_src;
}

- (void)midi_cc2_selected {
    NSLog(@"%s", __func__);
    midi_cc_selection = _midi_cc2_src;
}

- (void)midi_cc3_selected {
    NSLog(@"%s", __func__);
    midi_cc_selection = _midi_cc3_src;
}

- (void)midi_cc4_selected {
    NSLog(@"%s", __func__);
    midi_cc_selection = _midi_cc4_src;
}

- (void)midi_cc5_selected {
    NSLog(@"%s", __func__);
    midi_cc_selection = _midi_cc5_src;
}

- (void)midi_cc6_selected {
    NSLog(@"%s", __func__);
    midi_cc_selection = _midi_cc6_src;
}

- (void)midi_cc7_selected {
    NSLog(@"%s", __func__);
    midi_cc_selection = _midi_cc7_src;
}

- (void)midi_cc8_selected {
    NSLog(@"%s", __func__);
    midi_cc_selection = _midi_cc8_src;
}

- (void)midi_cc_handler:(int)num value:(int)val {
    
    // Select NSPopUpButton cell to the incoming MIDI CC number if the button menu is open
    if (midi_cc_selection) {
        dispatch_async(dispatch_get_main_queue(),^{
            [midi_cc_selection selectItemWithTag:num];
            [self midi_cc_selected:midi_cc_selection];
            midi_cc_selection = nil;
        });
    }
    
    if (num == 64) {
        lo_send(multicast_address, "/mod/egen/do_sus", "i", val < 63 ? 0 : 1);
        return;
    }
    else if (num == 59) {
        lo_send(multicast_address, "/propagate/kill", NULL);
    }
    else if (num == 75) {
        if (val > 63) {
            [_midiNoteAllocationModeControl setSelectedSegment:0];
        }
    }
    else if (num == 91) {
        if (val > 63)
            [_midiNoteAllocationModeControl setSelectedSegment:3];
    }
    else if (num == 80) {
        if (val > 63)
            [_propagation_enabled_check setState:NSOnState];
        else
            [_propagation_enabled_check setState:NSOffState];
        [self propagation_enabled_changed:_propagation_enabled_check];
    }
    else if (num == 80) {
        if (val > 63)
            [_follower_gate_enabled_check setState:NSOnState];
        else
            [_follower_gate_enabled_check setState:NSOffState];
        [self follower_gate_changed:_follower_gate_enabled_check];
    }
    else {
        int cc_idx;
        if (num == cc_nums[0])
            cc_idx = 0;
        else if (num == cc_nums[1])
            cc_idx = 1;
        else if (num == cc_nums[2])
            cc_idx = 2;
        else if (num == cc_nums[3])
            cc_idx = 3;
        else if (num == cc_nums[4])
            cc_idx = 4;
        else if (num == cc_nums[5])
            cc_idx = 5;
        else if (num == cc_nums[6])
            cc_idx = 6;
        else if (num == cc_nums[7])
            cc_idx = 7;
        else
            return;
        
        float min = cc_min[cc_idx];
        float max = cc_max[cc_idx];
        float scaled_val = (max-min) * (val/127.) + min;
        lo_send(multicast_address, [cc_paths[cc_idx] UTF8String], "f", scaled_val);
    }
}

- (IBAction)midi_cc_selected:(NSPopUpButton *)sender {
    cc_nums[(int)sender.tag] = (int)sender.selectedTag;
}

- (IBAction)midi_cc_dest_changed:(NSTextField *)sender {
    [cc_paths setObject:sender.stringValue atIndexedSubscript:sender.tag];
}

- (IBAction)midi_cc_min_changed:(NSTextField *)sender {
    cc_min[(int)sender.tag] = sender.floatValue;
}

- (IBAction)midi_cc_max_changed:(NSTextField *)sender {
    cc_max[(int)sender.tag] = sender.floatValue;
}

#pragma mark - Network Config
- (IBAction)multi_send_get_ip:(id)sender {
    node_addresses.clear();
    [node_windows removeAllObjects];
    lo_send(multicast_address, "/get_ip", NULL);
}

- (IBAction)multi_send_remove_listeners:(NSButton *)sender {
    lo_send(multicast_address, "/remove_listeners", NULL);
}

- (void)add_node:(const char *)address {
    
    bool match = false;
    for (int i = 0; i < node_addresses.size(); i++) {
        if (strcmp(address, lo_address_get_hostname(node_addresses[i])) == 0)
            match = true;
    }
    
    if (match)
        return;
    
    dispatch_sync(dispatch_get_main_queue(),^{      // Crash fix
        
        node_note_nums.push_back(-1);
        node_note_nums_previous.push_back(-1);
        node_addresses.push_back(lo_address_new(address, kLocal_Port));
        node_idx_randperm = [self arrange_randperm:(int)node_addresses.size()];         // Random
        node_idx_distributed = [self arrange_distributed:(int)node_addresses.size()];   // Distributed
        node_idx_hemispheric = [self arrange_hemispheric:(int)node_addresses.size()];   // Hemispheric
    
        NodeWindowController *win = [[NodeWindowController alloc]
                                     initWithWindowNibName:@"NodeWindowController"
                                     nodeAddress:node_addresses.back()
                                     multicastAddress:multicast_address];
        [node_windows addObject:win];
        
        [self print_nodes];
        [_nodeTableView reloadData];
    });
}

- (void)print_nodes {
    printf("available nodes:\n===========================\n");
    for (int i = 0; i < node_addresses.size(); i++) {
        printf("[%2d]    ip: %s\n", i, lo_address_get_hostname(node_addresses[i]));
        printf("[%2d]  port: %s\n", i, lo_address_get_port(node_addresses[i]));
    }
}

- (void)print_lo_address:(lo_address)address {
    printf("  ip: %s\n", lo_address_get_hostname(address));
    printf("port: %s\n", lo_address_get_port(address));
}

- (std::vector<lo_address>)get_node_addresses {
    std::vector<lo_address> addresses;
    addresses.push_back(local_address);
    addresses.push_back(multicast_address);
    lo_address broadcast_address = lo_address_new("10.0.1.255", kLocal_Port);
    addresses.push_back(broadcast_address);
    for (int i = 0; i < node_addresses.size(); i++) {
        addresses.push_back(node_addresses[i]);
    }
    return addresses;
}

#pragma mark - Propagation
- (IBAction)propagation_enabled_changed:(NSButton *)sender {
    lo_send(multicast_address, "/propagate/enable", "i", sender.state == NSOnState ? 1 : 0);
}

- (IBAction)follower_gate_changed:(NSButton *)sender {
    lo_send(multicast_address, "/mod/egen/follower_gate", "i", sender.state == NSOnState ? 1 : 0);
}

- (IBAction)propagation_decay_changed:(NSSlider *)sender {
    lo_send(multicast_address, "/propagate/decay", "f", sender.floatValue);
}

- (IBAction)propagation_kill_pressed:(NSButton *)sender {
    lo_send(multicast_address, "/propagate/kill", NULL);
}

- (IBAction)propagation_direction_set_cw:(NSButton *)sender {
//    lo_send(multicast_address, "/remove_listeners", NULL);
    [NSThread sleepForTimeInterval:0.1];
    int num_nodes = (int)node_addresses.size();
    int idx_source, idx_dest;
    for (int i = 0; i < num_nodes; i++) {
        idx_source = i;
        idx_dest = i+1 == num_nodes ? 0 : i+1;
        [self propagation_set_source:idx_source dest:idx_dest];
    }
}

- (IBAction)propagation_direction_set_ccw:(NSButton *)sender {
//    lo_send(multicast_address, "/remove_listeners", NULL);
    [NSThread sleepForTimeInterval:0.1];
    int num_nodes = (int)node_addresses.size();
    int idx_source, idx_dest;
    for (int i = num_nodes-1; i > -1; i--) {
        idx_source = i;
        idx_dest = i-1 == -1 ? num_nodes-1 : i-1;
        [self propagation_set_source:idx_source dest:idx_dest];
    }
}

- (IBAction)propagation_direction_set_bidir:(NSButton *)sender {
//    lo_send(multicast_address, "/remove_listeners", NULL);
    [NSThread sleepForTimeInterval:0.1];
    [self propagation_direction_set_cw:nil];
    [NSThread sleepForTimeInterval:0.1];
    [self propagation_direction_set_ccw:nil];
}

- (void)propagation_set_source:(int)src_idx dest:(int)dest_idx {
    
    const char *src_address = lo_address_get_hostname(node_addresses[src_idx]);
    int src_bytes[4];
    sscanf(src_address, "%d.%d.%d.%d", src_bytes, src_bytes+1, src_bytes+2, src_bytes+3);
    NSLog(@"Selected Source IP: %d.%d.%d.%d",
          src_bytes[0], src_bytes[1], src_bytes[2], src_bytes[3]);
    
    
    const char *dest_address = lo_address_get_hostname(node_addresses[dest_idx]);
    int dest_bytes[4];
    sscanf(dest_address, "%d.%d.%d.%d", dest_bytes, dest_bytes+1, dest_bytes+2, dest_bytes+3);
    NSLog(@"Selected Listener IP: %d.%d.%d.%d",
          dest_bytes[0], dest_bytes[1], dest_bytes[2], dest_bytes[3]);
    
    const char *dest_port = lo_address_get_port(node_addresses[dest_idx]);
    int dest_port_int;
    sscanf(dest_port, "%d", &dest_port_int);
    
    lo_send(node_addresses[src_idx], "/add_listener", "iiiii",
            dest_bytes[0], dest_bytes[1], dest_bytes[2], dest_bytes[3], dest_port_int);
}

- (IBAction)midi_device_selected:(NSPopUpButton *)sender {
    
    if (midiIn->isPortOpen()) {
        midiIn->closePort();
        midiIn->cancelCallback();
    }
    
    int portNum = (int)[[sender selectedItem] tag];
    midiIn->openPort(portNum);
    midiIn->setCallback(&midiCallback, (__bridge void*)self);
    midiIn->ignoreTypes(true, true, true);
}

#pragma mark - NSTableViewDataSource Methods
- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return (NSInteger)node_addresses.size();
}

- (id)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn
            row:(NSInteger)row {
    
    NSTextField *cellView = [tableView
                                 makeViewWithIdentifier:tableColumn.identifier
                                 owner:self];
    if (cellView == nil) {
        cellView = [[NSTextField alloc] initWithFrame:CGRectMake(0.0, 0.0, tableColumn.width, 10.0)];
        cellView.identifier = tableColumn.identifier;
    }
    
    // Node Number
    if ([[tableColumn identifier] isEqualToString:@"Node#"]) {
        cellView.integerValue = row;
        [cellView setAction:@selector(node_number_changed:)];
    }
    // IP Address
    else if ([[tableColumn identifier] isEqualToString:@"IPAddress"]) {
        cellView.stringValue =
        [NSString stringWithFormat:@"%s", lo_address_get_hostname(node_addresses[(int)row])];
    }
    // Port Number
    else if ([[tableColumn identifier] isEqualToString:@"Port#"]) {
        cellView.stringValue =
        [NSString stringWithFormat:@"%s", lo_address_get_port(node_addresses[(int)row])];
    }
    // Listener IP pop-up button
    else if ([[tableColumn identifier] isEqualToString:@"ListenerIPAddress"]) {
        NSPopUpButton *popCell = [[NSPopUpButton alloc] init];
        [popCell setAction:@selector(listenerSelected:)];
        popCell.identifier = [NSString stringWithFormat:@"%lu", row];
        for (int i = 0; i < node_addresses.size(); i++) {
            [popCell addItemWithTitle:[NSString stringWithFormat:@"%s",
                                       lo_address_get_hostname(node_addresses[(int)i])]];
        }
        return popCell;
    }
//    // Envelope follower trigger enable
//    else if ([[tableColumn identifier] isEqualToString:@"FollowerTrigger"]) {
//        NSButton *checkBox = [[NSButton alloc] init];
//        [checkBox setButtonType:NSSwitchButton];
//        [checkBox setTitle:@"Enabled"];
//        [checkBox setTag:row];
//        [checkBox setAction:@selector(followerTriggerEnableChanged:)];
//        return checkBox;
//    }
    // Test gate button
    else if ([[tableColumn identifier] isEqualToString:@"Test"]) {
        NSButton *button = [[NSButton alloc] init];
        [button setTitle:@"Test"];
        [button setTag:row];
        [button setAction:@selector(send_gate_on_test:)];
        return button;
    }
    // Open Paramaters button
    else if ([[tableColumn identifier] isEqualToString:@"Params"]) {
        NSButton *button = [[NSButton alloc] init];
        [button setTitle:@"Open"];
        [button setTag:row];
        [button setAction:@selector(openPressed:)];
        return button;
    }
    
    return cellView;
}

- (void)node_number_changed:(NSTextField *)sender {
    NSLog(@"%@", sender);
    NSLog(@"\t- value = %lu", sender.integerValue);
    
}

- (void)tableView:(NSTableView *)tableView sortDescriptorsDidChange:(NSArray *)oldDescriptors {
    
    if ([oldDescriptors count] == 0)
        return;
    
    NSSortDescriptor *desc = oldDescriptors[0];
    if (desc.ascending)
        std::sort(node_addresses.begin(), node_addresses.end(), lo_address_less_than_key());
    else
        std::sort(node_addresses.begin(), node_addresses.end(), lo_address_greater_than_key());
    
    [tableView reloadData];
}

// Custom sorts for c++ vector of lo_addresses
struct lo_address_less_than_key {
    inline bool operator() (const lo_address& add_1, const lo_address& add_2) {
        
        const char *adstr_1 = lo_address_get_hostname(add_1);
        int adbytes_1[4];
        sscanf(adstr_1, "%d.%d.%d.%d", adbytes_1, adbytes_1+1, adbytes_1+2, adbytes_1+3);
        
        const char *adstr_2 = lo_address_get_hostname(add_2);
        int adbytes_2[4];
        sscanf(adstr_2, "%d.%d.%d.%d", adbytes_2, adbytes_2+1, adbytes_2+2, adbytes_2+3);
        
        return (adbytes_1[3] < adbytes_2[3]);
    }
};

struct lo_address_greater_than_key {
    inline bool operator() (const lo_address& add_1, const lo_address& add_2) {
        
        const char *adstr_1 = lo_address_get_hostname(add_1);
        int adbytes_1[4];
        sscanf(adstr_1, "%d.%d.%d.%d", adbytes_1, adbytes_1+1, adbytes_1+2, adbytes_1+3);
        
        const char *adstr_2 = lo_address_get_hostname(add_2);
        int adbytes_2[4];
        sscanf(adstr_2, "%d.%d.%d.%d", adbytes_2, adbytes_2+1, adbytes_2+2, adbytes_2+3);
        
        return (adbytes_1[3] > adbytes_2[3]);
    }
};

#pragma mark - NSTableViewDelegate Methods
- (BOOL)tableView:(NSTableView *)tableView shouldSelectTableColumn:(NSTableColumn *)tableColumn {
    if (![tableColumn.identifier isEqualToString:@"ListenerIPAddress"])
        return NO;
    else return YES;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
    NSLog(@"%s", __func__);
}

- (void)listenerSelected:(NSPopUpButton *)sender {
    
    int row = [sender.identifier intValue];
    if (row < 0 || row > node_addresses.size())
        return;
    
    int list_idx = (int)[sender indexOfSelectedItem];
    
    const char *address = lo_address_get_hostname(node_addresses[list_idx]);
    int bytes[4];
    sscanf(address, "%d.%d.%d.%d", bytes, bytes+1, bytes+2, bytes+3);
    NSLog(@"Selected Listener IP: %d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
    
    const char *port = lo_address_get_port(node_addresses[list_idx]);
    int port_int;
    sscanf(port, "%d", &port_int);
    
    lo_send(node_addresses[row], "/add_listener", "iiiii",
            bytes[0], bytes[1], bytes[2], bytes[3], port_int);
}

- (void)openPressed:(NSButton *)sender {
    
    int row = (int)[sender tag];
    NSLog(@"%s: row = %d", __func__, row);
    if (row < 0 || row >= node_addresses.size())
        return;
    
    [node_windows[row] showWindow:self];
}

//- (void)followerTriggerEnableChanged:(NSButton *)sender {
//    
//    int row = (int)[sender tag];
//    if (row < 0 || row >= node_addresses.size())
//        return;
//    
//    NSLog(@"%s: IP: %s enabled (%s)", __func__, lo_address_get_hostname(node_addresses[row]), [sender state] == NSOnState ? "true" : "false");
//    
//    lo_send(node_addresses[row], "/mod/egen/follower_gate", "i", [sender state] == NSOnState ? 1 : 0);
//}


- (void)send_gate_on_test:(NSButton *)sender {
    
    int row = (int)[sender tag];
    if (row < 0 || row >= node_addresses.size())
        return;
    
    lo_send(node_addresses[row], "/mod/egen/gate", "i", 1);
    NSLog(@"%s: /mod/egen/gate", __func__);
}

@end
















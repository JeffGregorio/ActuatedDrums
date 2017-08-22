#include "NodeListenerArray.h"

NodeListenerArray::NodeListenerArray() {}

/**
 * Add an OSC listener to the list using the provided IP and port.
 */
void NodeListenerArray::handle_add_listener(OSCMessage &msg) {

  // Validate message size
  int msg_size = msg.size();                  
  bool valid = msg_size == 5;
  if (!valid)                                 
    return; 

  // Validate IP format
  uint8_t address_bytes[4];
  for (int i = 0; i < msg_size-1; i++) {      
    if (msg.isInt(i)) 
      address_bytes[i] = msg.getInt(i);
    else 
      return;
  }
  
  // Validate port format
  if (!msg.isInt(4))                          
    return;

  // Create new OSC listener
  OscListener newListener;                    
  newListener.ip = IPAddress(address_bytes);
  newListener.port = msg.getInt(4);

  // Add new listener to list if it's not a duplicate
  if (match_listener(newListener.ip, newListener.port) == -1) 
    osc_listeners.push_back(newListener);

#ifdef DEBUG_PRINT
  print_listeners();
#endif
}

/**
 * Remove any OSC listeners from the list matching the given IP and port.
 */
void NodeListenerArray::handle_remove_listener(OSCMessage &msg) {

  // Validate message size
  int msg_size = msg.size();
  bool valid = msg_size == 5;
  if (!valid)   
    return; 

  // Validate IP format
  uint8_t address_bytes[4];
  for (int i = 0; i < msg_size-1; i++) {
    if (msg.isInt(i)) 
      address_bytes[i] = msg.getInt(i);
    else 
      return;
  }

  // Validate port format
  int port;
  if (msg.isInt(4)) 
    port = msg.getInt(4);
  else
    return;

  // Erase any OSC listeners with matching IP/port
  int idx = match_listener(IPAddress(address_bytes), port);
  if (idx != -1)
    osc_listeners.erase(osc_listeners.begin()+idx);

#ifdef DEBUG_PRINT
  print_listeners();
#endif
}

void NodeListenerArray::handle_remove_listeners(OSCMessage &msg) {
  osc_listeners.clear();
#ifdef DEBUG_PRINT
  print_listeners();
#endif
}

/**
 * Check a given IP and port number against current OSC listeners and 
 * return the index of any matching IP/port pair.
 */
int NodeListenerArray::match_listener(IPAddress ip, int port) {
  int idx = -1;
  for (int i = 0; i < osc_listeners.size(); i++) {
    bool matched = true;
    for (int j = 0; j < 4; j++) {
      if (ip[j] != osc_listeners[i].ip[j])          // Match IP
        matched = false;
    }
    if (matched && port != osc_listeners[i].port)   // Match port number
      matched = false;
    if (matched)
      idx = i;
  }
  return idx;
}

/**
 * Print IP addresses and port numbers of any OSC listeners.
 */
void NodeListenerArray::print_listeners() {
  Serial.println();
  Serial.print(osc_listeners.size());
  Serial.println(" OSC listeners");
  for (int i = 0; i < osc_listeners.size(); i++) {
    Serial.print("OSC Listener ");
    Serial.print(i);
    Serial.println(":");
    Serial.print("  IP: ");
    Serial.println(osc_listeners[i].ip);
    Serial.print("PORT: ");
    Serial.println(osc_listeners[i].port);
  }
}


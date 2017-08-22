/* NodeListenerArray.h
 *  
 *  Manages a list of listener IP address/port pairs. Used to propagate gates and
 *  parameters from this node to its listeners. 
 */
#include <OSCMessage.h>
#include <IPAddress.h>
#include <vector>

#define DEBUG_PRINT

typedef struct OscListener {  // IPAddress, port# pairs
  IPAddress ip;
  int port;
} OscListener;

class NodeListenerArray {

public:
  NodeListenerArray();
  
  void handle_add_listener(OSCMessage &msg);
  void handle_remove_listener(OSCMessage &msg);
  void handle_remove_listeners(OSCMessage &msg);
  
  int count() { return osc_listeners.size(); }
  OscListener operator[](const int idx) { return osc_listeners[idx]; }

private:
  int match_listener(IPAddress ip, int port);
  void print_listeners();

  std::vector<OscListener> osc_listeners;
};




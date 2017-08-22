//#define DEBUG_PRINT

#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <OSCMessage.h>
#include <SLIPEncodedSerial.h>
#include <vector>

extern "C" {
  #include "user_interface.h"
}

const int baudrate = 230400;
const int PIN_LED = 2;

/* Network name and password */
const char* ssid = "Drumhenge";
const char* pass = "ahengeofdrums";

// Local IP and UDP instance
IPAddress ipLocal;
WiFiUDP udpLocal;
unsigned int portLocal = 7770; 
IPAddress udpRemoteIP(0, 0, 0, 0);

// Multicast IP and UDP instance
/* Multicast IP range: [224.0.0.0, 239.255.255.255]
 *  [224.0.0.0, 224.0.0.255]      Reserved for special "well-known" multicast addresses
 *  [224.0.1.0, 238.255.255.255]  Globally-scoped (internet-wide) multicast addresses
 *  [239.0.0.0, 239.255.255.255]  Administratively-scoped (local) multicast addresses
 */
IPAddress ipMulti(239, 0, 0, 1); 
WiFiUDP udpMulti;
unsigned int portMulti = 7771;
unsigned long udpMultiMsgTime_ms = 0;
unsigned long udpMultiLEDTime_ms = 20;

// Outgoing OSC Messaging
OSCErrorCode error;
IPAddress outgoing_dest(0, 0, 0, 0);
unsigned int outgoing_port = portLocal;
OSCMessage outgoing_msg;

// SLIP Serial (ESP8266 <-> Teensy 3.2)
SLIPEncodedSerial SLIPSerial(Serial);   

void setup() {

  // WIFI Indicator LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  
  for (int i = 0; i < 4; i++) {
    digitalWrite(PIN_LED, HIGH);
    delay(100);
    digitalWrite(PIN_LED, LOW);
    delay(100);
  }

  // Open serial communication to Teensy
  Serial.begin(baudrate);

  // Setting up Station AP and UDP ports
  connect_wifi();
  open_port_local();
  open_port_multi();
}

void loop() {
  
  handle_udp_local();     // OSC messages to local IP
  handle_udp_multi();     // OSC messages to multicast IP
  handle_osc_slip();      // OSC messages from Teensy to listeners

  if (WiFi.status() != WL_CONNECTED) 
    connect_wifi();
  else if (millis()-udpMultiMsgTime_ms > udpMultiLEDTime_ms)
    digitalWrite(PIN_LED, HIGH);
}

/* ------------ */
/* === WIFI === */
/* ------------ */
bool connect_wifi() {

  WiFi.begin(ssid, pass);

  // Attempt to connect to the specified netowrk up to 30 times
  int tries = 0;
  bool led_on = true;
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(PIN_LED, led_on ? HIGH : LOW);   // Blink the LED while connecting
    led_on = !led_on;
    tries++;
    if (tries > 100) {
      digitalWrite(PIN_LED, LOW);   // Turn LED off to indicate failure
      return false;
    }
    delay(500);
  }

  ipLocal = WiFi.localIP();     // Get IP address
  digitalWrite(PIN_LED, HIGH);  // Turn LED on to indicate success

  return true;
}

/* ------------------- */
/* === UDP (setup) === */
/* ------------------- */
bool open_port_local() {
  bool success;
  success = udpLocal.begin(portLocal) == 1;
  if (success) {
    OSCMessage msg("/set_port/local");
    for (int i = 0; i < 4; i++)  
      msg.add((uint32_t)WiFi.localIP()[i]);
    msg.add(portLocal);
    slip_send(msg);    
  }
  else 
    slip_send_debug("Failed to open UDP port (local)");
  return success;
}

bool open_port_multi() {
  bool success;
  success = udpMulti.beginMulticast(WiFi.localIP(), ipMulti, portMulti) == 1;
  if (success) {
    OSCMessage msg("/set_port/multi");
    for (int i = 0; i < 4; i++)  
      msg.add((uint32_t)ipMulti[i]);
    msg.add(portMulti);
    slip_send(msg);
  }
  else 
    slip_send_debug("Failed to open UDP port (multicast)");
  return success;
}

/* ---------------------------- */
/* === UDP (event handlers) === */
/* ---------------------------- */
void handle_udp_local() {
  
  int n_bytes = udpLocal.parsePacket();   // Number of available UDP bytes
  if (n_bytes) {
    
    // Create OSC message from UDP bytes
    OSCMessage msg; 
    while (n_bytes--) 
      msg.fill(udpLocal.read());

    if (!msg.hasError())  {

      // If the remote IP has changed, notify the Teensy of the most recent
      if (udpRemoteIP != udpLocal.remoteIP()) {
        udpRemoteIP = udpLocal.remoteIP();
        OSCMessage ripMsg("/remote_ip");
        for (int i = 0; i < 4; i++)  
          ripMsg.add((uint32_t)udpLocal.remoteIP()[i]);
        slip_send(ripMsg);
      }      
      
      slip_send(msg);   // ESP --> Teensy via SLIPSerial
      digitalWrite(PIN_LED, LOW);
      udpMultiMsgTime_ms = millis();
    }
    else 
      error = msg.getError();
  }  
}

void handle_udp_multi() {
  
  int n_bytes = udpMulti.parsePacket();   // Number of available UDP bytes
  if (n_bytes) {
    
    // Create OSC message from UDP bytes
    OSCMessage msg; 
    while (n_bytes--) 
      msg.fill(udpMulti.read());
  
    if (!msg.hasError()) {

      // If the remote IP has changed, notify the Teensy of the most recent
      if (udpRemoteIP != udpMulti.remoteIP()) {
        udpRemoteIP = udpMulti.remoteIP();
        OSCMessage ripMsg("/remote_ip");
        for (int i = 0; i < 4; i++)  
          ripMsg.add((uint32_t)udpMulti.remoteIP()[i]);
        slip_send(ripMsg);
      }  
      
      if (msg.dispatch("/get_ip", handle_getip)) {}   // Handle local IP requests
      else slip_send(msg);                            // ESP --> Teensy via SLIPSerial
      digitalWrite(PIN_LED, LOW);
      udpMultiMsgTime_ms = millis();
    }
    else 
      error = msg.getError();
  }  
}

/**
 * Handle OSC multicast requests for local IP. Create OSC message contatining local IP and 
 * send back to the IP/port that send the request.
 */
void handle_getip(OSCMessage &msg) {
    OSCMessage response("/ip");
    for (int i = 0; i < 4; i++)  
      response.add((int32_t)WiFi.localIP()[i]);
    udpMulti.beginPacket(udpMulti.remoteIP(), portMulti);   
    response.send(udpMulti);
    udpMulti.endPacket(); 
}

/* ------------------ */
/* === SLIPSerial === */
/* ------------------ */
void slip_send(OSCMessage &msg) {
  SLIPSerial.beginPacket();
  msg.send(SLIPSerial);
  SLIPSerial.endPacket();
}

void slip_send_debug(const char *string) {
  OSCMessage debug_msg("/debug");
  debug_msg.add(string);
  slip_send(debug_msg);
}

void slip_send_debug(int intValue) {
  OSCMessage debug_msg("/debug");
  debug_msg.add(intValue);
  slip_send(debug_msg);
}

void slip_send_debug(float floatValue) {
  OSCMessage debug_msg("/debug");
  debug_msg.add(floatValue);
  slip_send(debug_msg);
}

/**
 * Parse incoming OSC messages from Teensy and relay them to the multicast port or 
 * an address specified by setting 'outgoing_dest'.
 */
void handle_osc_slip() {

  OSCMessage msg;
  int msg_size = SLIPSerial.available();
  if (msg_size == 0)
    return;

  while (!SLIPSerial.endofPacket()) {
    if ((msg_size = SLIPSerial.available()) > 0) {
      while (msg_size--)
        msg.fill(SLIPSerial.read());
    }
  }

  if (!msg.hasError()) {
    if (msg.dispatch("/set_dest", handle_set_dest)) {}   // 
    else send_osc_local(msg);
  }
}

void handle_set_dest(OSCMessage &msg) {
  if (msg.isInt(0) && msg.isInt(1) && msg.isInt(2) && msg.isInt(3) && msg.isInt(4)) {
    uint8_t ip_bytes[4];
    for (int i = 0; i < 4; i++) 
      ip_bytes[i] = msg.getInt(i);
    outgoing_dest = IPAddress(ip_bytes);
    outgoing_port = msg.getInt(4);
  }
}

void send_osc_local(OSCMessage &msg) {
  udpLocal.beginPacket(outgoing_dest, outgoing_port);
  msg.send(udpLocal);
  udpLocal.endPacket();
}


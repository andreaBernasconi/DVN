/*
Title: DVN Opta
Board: Opta
*/


#include <Ethernet.h>
#include <PortentaEthernet.h>
#include <SPI.h>
#include <EthernetUDP.h>
#include "opta_info.h"
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <OSCData.h>

#define UDP_TX_PACKET_MAX_SIZE 1024
#define GET_OPTA_OTP_BOARD_INFO


OptaBoardInfo* info;
OptaBoardInfo* boardInfo();

EthernetUDP Udp;
unsigned int localPort = 8888;

IPAddress pcIP(192, 168, 100, 53);  // PC IP address
const unsigned int pcPort = 8888;   // PC UDP port

char packetBuffer[UDP_TX_PACKET_MAX_SIZE];

byte systemStatus = 0;  // 0 off, 1 on, 2 standby, 3 reserved

OSCBundle bndl;

// Wake-on-LAN
const byte first_six_bytes[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
EthernetUDP udp_client;
const byte broadcast_ip[] = { 255, 255, 255, 255 };
const byte target_mac[] = { 0x00, 0x23, 0x24, 0xBC, 0x9E, 0x51 };

// debounce
const unsigned long debounceDelay = 300;

// pins using debounce (buttons only)
const int debouncePins[] = { A0, A1, A2 };
const int NUM_DEBOUNCE_PINS = sizeof(debouncePins) / sizeof(debouncePins[0]);

int debounceIndex(int pin) {
  for (int i = 0; i < NUM_DEBOUNCE_PINS; i++) {
    if (debouncePins[i] == pin) return i;
  }
  return -1;
}

bool debounce(int pin) {
  static unsigned long lastTime[10] = { 0 };
  static int lastState[10] = { 0 };

  int idx = debounceIndex(pin);
  if (idx < 0) return false;

  int state = digitalRead(pin);

  if (state != lastState[idx]) {
    if (state == HIGH && (millis() - lastTime[idx]) > debounceDelay) {
      lastTime[idx] = millis();
      lastState[idx] = state;
      return true;
    }
    lastState[idx] = state;
  }
  return false;
}

// input pins
int onOffBtn = A0;     // power button
int startBtnIta = A1;  // Italian Button
int startBtnEng = A2;  // English Button

// relay outputs
int onOffRel = D0;
int ampRel = D1;

// Opta LEDs
int grnLed = LED_RESET;
int redLed = LEDR;
int onOffRelLed = LED_D0;
int ampRelLed = LED_D1;

void setup() {

  pinMode(onOffBtn, INPUT_PULLDOWN);
  pinMode(startBtnIta, INPUT_PULLDOWN);
  pinMode(startBtnEng, INPUT_PULLDOWN);

  pinMode(onOffRel, OUTPUT);
  pinMode(ampRel, OUTPUT);

  pinMode(onOffRelLed, OUTPUT);
  pinMode(ampRelLed, OUTPUT);

  pinMode(grnLed, OUTPUT);
  pinMode(redLed, OUTPUT);
  pinMode(BTN_USER, INPUT);

  // Ethernet configuration
  IPAddress ip(192, 168, 100, 61);
  IPAddress myDns(192, 168, 100, 254);
  IPAddress gateway(192, 168, 100, 254);
  IPAddress subnet(255, 255, 255, 0);

  Ethernet.begin(boardInfo()->mac_address, ip, myDns, gateway, subnet);
  Udp.begin(localPort);
}

void loop() {
  onOffBtnHandler();
  startBtnItaHandler();
  startBtnEngHandler();
  packedInHandler();
}

// -----------------------------------------------------------------------------
// BUTTON HANDLERS
// -----------------------------------------------------------------------------

void onOffBtnHandler() {
  if (systemStatus != 2) {
    if (debounce(onOffBtn)) {
      if (systemStatus == 0) switchOn();
      else if (systemStatus == 1) switchOff();
    }
  }
}

void startBtnItaHandler() {
  if (systemStatus == 1) {
    if (debounce(startBtnIta)) {
      bndl.add("/startBtnIta").add(1);
      bundle_send(&bndl);
    }
  }
}

void startBtnEngHandler() {
  if (systemStatus == 1) {
    if (debounce(startBtnEng)) {
      bndl.add("/startBtnEng").add(1);
      bundle_send(&bndl);
    }
  }
}

// -----------------------------------------------------------------------------
// SWITCH CONTROL
// -----------------------------------------------------------------------------

void switchOn() {
  systemStatus = 2;
  digitalWrite(onOffRelLed, HIGH);
  digitalWrite(onOffRel, HIGH);
  digitalWrite(redLed, HIGH);
  delay(5500);
  send_wol_packet();
}

void switchOff() {
  systemStatus = 2;
  digitalWrite(ampRel, LOW);
  digitalWrite(ampRelLed, LOW);
  digitalWrite(grnLed, LOW);
  digitalWrite(redLed, HIGH);
  bndl.add("/quitSystem");
  bundle_send(&bndl);
}

// -----------------------------------------------------------------------------
// OSC SEND
// -----------------------------------------------------------------------------

void bundle_send(OSCBundle* bndl) {
  Udp.beginPacket(pcIP, pcPort);
  bndl->send(Udp);
  Udp.endPacket();
  bndl->empty();
}

// -----------------------------------------------------------------------------
// WAKE-ON-LAN
// -----------------------------------------------------------------------------

void send_wol_packet() {
  const int wolRetries = 3;  // number of WOL packets
  const int wolDelay = 200;  // delay between packets (ms)

  for (int r = 0; r < wolRetries; r++) {
    udp_client.begin(7);
    udp_client.beginPacket(broadcast_ip, 7);

    // magic header
    udp_client.write(first_six_bytes, sizeof first_six_bytes);

    // repeat MAC 16 times
    for (int i = 0; i < 16; i++) {
      udp_client.write(target_mac, sizeof target_mac);
    }

    udp_client.endPacket();
    udp_client.stop();

    delay(wolDelay);  // spacing between packets
  }
}


// -----------------------------------------------------------------------------
// OSC RECEIVE
// -----------------------------------------------------------------------------

void packedInHandler() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    OSCBundle bundleIn;
    for (int i = 0; i < packetSize; i++) {
      bundleIn.fill(packetBuffer[i]);
    }
    if (!bundleIn.hasError()) {
      bundleIn.route("/maxSystemState", maxSystemState);
      bundleIn.route("/alive", aliveHandler);
    }
    bundleIn.empty();
  }
}





void maxSystemState(OSCMessage& msg, int addrOffset) {
  int maxSystemState = msg.getInt(0);

  switch (maxSystemState) {

    case 0:
      // Max is shutting down
      digitalWrite(redLed, LOW);
      delay(25000);
      systemStatus = 0;
      digitalWrite(onOffRelLed, LOW);
      digitalWrite(onOffRel, LOW);
      break;



    case 1:  // Max is running
      digitalWrite(ampRel, HIGH);
      digitalWrite(ampRelLed, HIGH);
      delay(1000);
      systemStatus = 1;
      digitalWrite(redLed, LOW);
      digitalWrite(grnLed, HIGH);
      break;
  }
}

void aliveHandler(OSCMessage& msg, int addrOffset) {
  if (systemStatus == 0) {   
    return;
  }

  int aliveNumber = -1;
  if (msg.size() == 1) aliveNumber = msg.getInt(0);
  bndl.add("/alive_ack").add("opta").add(aliveNumber);
  bundle_send(&bndl);
}

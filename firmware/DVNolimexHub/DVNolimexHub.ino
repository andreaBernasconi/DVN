/*
Title: DVN Olimex HUB
Board: OLIMEX ESP32-POE
 - Flash Size:        4MB
 - Partition Scheme:  Huge APP (3MB No OTA)   [REQUIRED]
 - CPU Frequency:     240 MHz
 - PSRAM:             Disabled
*/
#include <Logging.h>
#include <OscUtils.h>
#include <Utils.h>
#include <ETH.h>
#include <WiFi.h>
#include <WiFiGeneric.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include "RoutingTable.h"

#include <RadioConfig.h>

RadioConfig radioConfig;

#define ESPNOW_MAX_PAYLOAD 250
#define MAX_ROUTES 10

const unsigned long ALIVE_TIMEOUT_MS = 4000;

WiFiUDP Udp;

// Ethernet Configuration
const IPAddress ROUTER_IP(192, 168, 100, 71);
const IPAddress ROUTER_GATEWAY(192, 168, 100, 254);
const IPAddress ROUTER_SUBNET(255, 255, 255, 0);
const int LOCAL_OSC_PORT = 8888;

#define ETH_TYPE ETH_PHY_LAN8720
#define ETH_ADDR 0
#define ETH_MDC 23
#define ETH_MDIO 18
#define ETH_POWER 12
#define ETH_CLKMODE ETH_CLOCK_GPIO17_OUT

// PC
IPAddress PcIP(192, 168, 100, 51);
int PcPort = 8888;

bool onlineStatus[MAX_ROUTES] = { false };

// ESP-NOW diagnostics counters
volatile uint32_t espnowInvalidPackets = 0;
volatile uint32_t espnowTruncatedPackets = 0;
volatile uint32_t espnowUnknownMac = 0;

static uint32_t lastDiag = 0;
static const uint32_t DIAG_INTERVAL = 10000;

// sendOscToPC
void sendOscToPc(OSCMessage &msg) {
  Udp.beginPacket(PcIP, PcPort);
  msg.send(Udp);
  Udp.endPacket();
}

// sendStatusToMax
void sendStatusToMax() {
  for (int i = 0; i < routingTableSize && i < MAX_ROUTES; i++) {
    if (!routingTable[i].enabled) continue;

    OSCMessage msg("/alive_ack");
    msg.add(routingTable[i].prefix);
    msg.add(onlineStatus[i] ? 1 : 0);

    sendOscToPc(msg);
  }
  LOGLN("Status sent to MAX");
}

// Handle /olimex/radio/channel
void handleChannel(OSCMessage &msg, int addrOffset) {
  int newChannel = msg.getInt(0);
  radioConfig.channel = newChannel;
  saveRadioConfig(radioConfig);
  applyRadioConfig(radioConfig);

  OSCMessage resp("/radio/channel");
  resp.add("olimex");
  resp.add(newChannel);
  sendOscToPc(resp);

  LOG("Channel updated to ");
  LOGLN(newChannel);
}

// Handle /olimex/radio/power
void handlePower(OSCMessage &msg, int addrOffset) {
  int newPower = msg.getInt(0);
  radioConfig.txPower = newPower;
  saveRadioConfig(radioConfig);
  applyRadioConfig(radioConfig);

  OSCMessage resp("/radio/power");
  resp.add("olimex");
  resp.add(newPower);
  sendOscToPc(resp);
}

// Handle /olimex/radio/status
void handleRadioStatus(OSCMessage &msg, int addrOffset) {
  OSCMessage ch("/radio/channel");
  ch.add("olimex");
  ch.add(radioConfig.channel);
  sendOscToPc(ch);

  OSCMessage pw("/radio/power");
  pw.add("olimex");
  pw.add(radioConfig.txPower);
  sendOscToPc(pw);
}

// Handle /olimex/status/request
void handleStatusRequest(OSCMessage &msg, int addrOffset) {
  sendStatusToMax();
}

// Handle incoming /olimex/alive
void handleOlimexAlive(OSCMessage &msg, int addrOffset) {

  int aliveNumber = msg.getInt(0);
  static uint8_t outBuffer[ESPNOW_MAX_PAYLOAD];

  for (int i = 0; i < routingTableSize; i++) {
    if (!routingTable[i].enabled) continue;

    OSCMessage alive("/alive");

    int outLen = buildOscForEspNow(alive, outBuffer, ESPNOW_MAX_PAYLOAD);
    if (outLen == 0) continue;

    ensurePeer(routingTable[i].mac, radioConfig.channel);
    sendEspNow(routingTable[i].mac, outBuffer, outLen);
  }

  OSCMessage reply("/alive_ack");
  reply.add("olimex").add(aliveNumber);
  sendOscToPc(reply);
}

bool isKnownMac(const uint8_t *mac) {
  for (int i = 0; i < routingTableSize; i++) {
    if (!routingTable[i].enabled) continue;
    if (memcmp(mac, routingTable[i].mac, 6) == 0) return true;
  }
  return false;
}

// ESP-NOW receive callback
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {

  if (len <= 0 || len > ESPNOW_MAX_PAYLOAD) {
    espnowInvalidPackets++;

    OSCMessage err("/error");
    err.add("espnow_packet_invalid");
    err.add(len);
    sendOscToPc(err);

    LOGLN("ESP-NOW packet size invalid");
    return;
  }

if (len < 8) {
  espnowTruncatedPackets++;
  LOGLN("ESP-NOW packet too short, discarded");
  return;
}


  if (!isKnownMac(info->src_addr)) {
    espnowUnknownMac++;
    LOGLN("ESP-NOW unknown MAC");
    return;
  }

  static uint8_t temp[ESPNOW_MAX_PAYLOAD];
  memcpy(temp, data, len);

  OSCMessage inMsg;
  inMsg.fill(temp, len);

  if (inMsg.hasError()) {
    espnowInvalidPackets++;
    LOGLN("OSC decode error");
    return;
  }

  const char *prefix = findPrefixByMac(info->src_addr);
  if (!prefix) {
    espnowUnknownMac++;
    LOGLN("Unknown MAC prefix");
    return;
  }

  char cmd[64];
  inMsg.getAddress(cmd);

  if (strcmp(cmd, "/alive_ack") == 0) {
    RouteEntry *route = findRouteByMac(info->src_addr);
    if (route) route->lastAlive = millis();
    return;
  }

  OSCMessage outMsg(cmd);
  outMsg.add(prefix);
  oscCopyArgs(inMsg, outMsg);

  sendOscToPc(outMsg);
}

// Event-driven ESP-NOW error reporting
void espnowErrorUpdate() {
  uint32_t now = millis();
  if (now - lastDiag < DIAG_INTERVAL) return;

  lastDiag = now;

  if (espnowInvalidPackets == 0 && espnowTruncatedPackets == 0 && espnowUnknownMac == 0) return;

  OSCMessage msg("/error");

  if (espnowInvalidPackets > 0) {
    msg.add("espnow_invalid");
    msg.add((int)espnowInvalidPackets);
  }

  if (espnowTruncatedPackets > 0) {
    msg.add("espnow_truncated");
    msg.add((int)espnowTruncatedPackets);
  }

  if (espnowUnknownMac > 0) {
    msg.add("espnow_unknown_mac");
    msg.add((int)espnowUnknownMac);
  }

  sendOscToPc(msg);

  espnowInvalidPackets = 0;
  espnowTruncatedPackets = 0;
  espnowUnknownMac = 0;
}

// Check alive timeout
void checkAliveTimeout() {
  static unsigned long lastAliveCheck = 0;
  unsigned long now = millis();

  if (now - lastAliveCheck < 1000) return;
  lastAliveCheck = now;

  for (int i = 0; i < routingTableSize; i++) {
    if (!routingTable[i].enabled) continue;

    bool online = routingTable[i].lastAlive > 0 && (now - routingTable[i].lastAlive) < ALIVE_TIMEOUT_MS;

    if (online != onlineStatus[i]) {
      onlineStatus[i] = online;

      OSCMessage msg("/alive_ack");
      msg.add(routingTable[i].prefix);
      msg.add(online ? 1 : 0);
      sendOscToPc(msg);

      LOG(routingTable[i].prefix);
      LOG(online ? " → ONLINE" : " → OFFLINE");
      LOGLN("");
    }
  }
}

// Ethernet event handler
void onEthEvent(arduino_event_id_t event) {
  switch (event) {

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      LOGLN("Ethernet disconnected");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      LOGLN("Ethernet link up");
      Udp.begin(LOCAL_OSC_PORT);
      sendStatusToMax();
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      LOGLN("Ethernet got IP");
      break;

    default:
      break;
  }
}

// Setup
void setup() {
  Serial.begin(115200);
  delay(200);

  loadRadioConfig(radioConfig);

  Network.onEvent(onEthEvent);

  ETH.begin(ETH_TYPE, ETH_ADDR, ETH_MDC, ETH_MDIO, ETH_POWER, ETH_CLKMODE);
  ETH.config(ROUTER_IP, ROUTER_GATEWAY, ROUTER_SUBNET);

  LOGLN("Waiting for Ethernet link...");

  unsigned long ethStart = millis();
  while (!ETH.linkUp() && millis() - ethStart < 10000) {
    LOGLN(".");
    delay(300);
  }

  if (!ETH.linkUp()) LOGLN("Ethernet timeout");
  else LOGLN("Ethernet OK");

  WiFi.mode(WIFI_STA);

  applyRadioConfig(radioConfig);

  if (esp_now_init() != ESP_OK) {
    LOGLN("ESP-NOW init error");
    return;
  }

  esp_now_register_recv_cb(onEspNowRecv);

  for (int i = 0; i < routingTableSize; i++) {
    if (!routingTable[i].enabled) continue;

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, routingTable[i].mac, 6);
    peer.channel = 0;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) == ESP_OK) {
      LOG("Peer added: ");
      LOGLN(routingTable[i].prefix);
    } else {
      LOG("Peer add error: ");
      LOGLN(routingTable[i].prefix);
    }
  }

  LOGLN("Router ready");
  sendStatusToMax();
}

// Loop
void loop() {
  checkAliveTimeout();
  espnowErrorUpdate();

  int packetSize = Udp.parsePacket();
  if (packetSize <= 0) return;

  if (packetSize > 512) {
    OSCMessage err("/error");
    err.add("udp_packet_too_large");
    err.add(packetSize);
    sendOscToPc(err);

    while (Udp.available()) Udp.read();
    return;
  }

  static uint8_t buffer[512];
  int len = Udp.read(buffer, sizeof(buffer));

  OSCBundle bundle;
  bundle.fill(buffer, len);

  if (bundle.hasError()) {
    OSCMessage err("/error");
    err.add("osc_parse_error");
    sendOscToPc(err);
    return;
  }

  int count = bundle.size();
  for (int i = 0; i < count; i++) {
    OSCMessage msg = bundle.getOSCMessage(i);

    char address[128];
    msg.getAddress(address);

    if (msg.route("/olimex/radio/channel", handleChannel)) continue;
    if (msg.route("/olimex/radio/power", handlePower)) continue;
    if (msg.route("/olimex/status/request", handleStatusRequest)) continue;
    if (msg.route("/olimex/alive", handleOlimexAlive)) continue;
    if (msg.route("/olimex/radio/status", handleRadioStatus)) continue;

    char prefix[32];
    char command[64];

    if (!parseOscAddress(address, prefix, sizeof(prefix), command, sizeof(command))) {
      LOG("Invalid OSC address: ");
      LOGLN(address);
      continue;
    }

    RouteEntry *route = nullptr;
    int routeIndex = -1;

    if (!findRouteSafe(prefix, route, routeIndex)) {
      LOG("Unknown prefix: ");
      LOGLN(prefix);
      continue;
    }

    OSCMessage outMsg = oscBuildMessage(command, msg);

    static uint8_t outBuffer[ESPNOW_MAX_PAYLOAD];
    int outLen = buildOscForEspNow(outMsg, outBuffer, ESPNOW_MAX_PAYLOAD);

    if (outLen == 0) {
      LOG("Invalid or oversized OSC payload");
      continue;
    }

    if (!onlineStatus[routeIndex]) {
      LOG("Board offline: ");
      LOGLN(route->prefix);
      continue;
    }

    ensurePeer(route->mac, radioConfig.channel);
    sendEspNow(route->mac, outBuffer, outLen);
  }
}

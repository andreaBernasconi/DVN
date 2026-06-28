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

const char* Version = "0.0.1";

OptaBoardInfo* info;
OptaBoardInfo* boardInfo();

EthernetUDP Udp;
unsigned int localPort = 8888;

IPAddress pcIP(192, 168, 100, 53);  //PcIp 900 -03
const unsigned int pcPort = 8888;   // PcPort

char packetBuffer[UDP_TX_PACKET_MAX_SIZE];

byte systemStatus = 0;  // 0  off, 1 On, 2 standBy, 3 Set da fare

OSCBundle bndl;


//wol
const byte first_six_bytes[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
EthernetUDP udp_client;
const byte broadcast_ip[] = { 255, 255, 255, 255 };                // Broadcast IP address
const byte target_mac[] = { 0x00, 0x23, 0x24, 0xBC, 0x9E, 0x51 };  // PC's MAC address

//debounce
unsigned long debounceDelay = 300;
unsigned long checkDelay = 1000;

//input name
int onOffBtn = A0; //T1
int moveSns = A1; //T2
int startBtn = A2; //T3

//Relay
int onOffRel = D0;
int ampRel = D1;

// optaLed
int grnLed = LED_RESET;  //green
int redLed = LEDR;       //red
int onOffRelLed = LED_D0;
int ampRelLed = LED_D1;



void setup() {

  //Serial.begin(115200);


  pinMode(onOffBtn, INPUT_PULLDOWN);  // button OnOff
  pinMode(moveSns, INPUT_PULLDOWN);   // contatto sensore
  pinMode(startBtn, INPUT_PULLDOWN);  // start button

  pinMode(onOffRel, OUTPUT);  //onOff Relay
  pinMode(ampRel, OUTPUT);    //amp Relay



  pinMode(onOffRelLed, OUTPUT);  // onOffRel Led
  pinMode(ampRelLed, OUTPUT);    // ampRel Led


  pinMode(grnLed, OUTPUT);   // reset led opta
  pinMode(redLed, OUTPUT);   // red led opta
  pinMode(BTN_USER, INPUT);  // spare




  //OPTA IP CONFIG
  IPAddress ip(192, 168, 100, 61);        //static address
  IPAddress myDns(192, 168, 100, 254);    // same as gateway
  IPAddress gateway(192, 168, 100, 254);  // gateway
  IPAddress subnet(255, 255, 255, 0);     // subnet
  Ethernet.begin(boardInfo()->mac_address, ip, myDns, gateway, subnet);
  Udp.begin(localPort);
}

void loop() {
  onOffBtnHandler();
  startBtnHandler();
  moveSnsHandler();
  packedInHandler();
}


// function

void onOffBtnHandler() {
  if (systemStatus != 2) {
    static unsigned long timeAtPress = 0;
    if (digitalRead(onOffBtn)) {
      if ((millis() - timeAtPress) > debounceDelay) {
        switch (systemStatus) {
          case 0:
            switchOn();
            break;
          case 1:
            switchOff();
            break;
        }
      }
      timeAtPress = millis();
    }
  }
}

void startBtnHandler() {
  if (systemStatus == 1) {
    static unsigned long timeAtPress = 0;
    if (digitalRead(startBtn)) {
      if ((millis() - timeAtPress) > debounceDelay) {
        bndl.add("/startBtn").add((int)1);
        bundle_send(&bndl);
      }
      timeAtPress = millis();
    }
  }
}

void moveSnsHandler() {
  if (systemStatus == 1) {
    static int switchState = 2;
    if (digitalRead(moveSns) == 1 && switchState != 1) {
      bndl.add("/moveSns").add(1);
      bundle_send(&bndl);
      switchState = 1;
    } else if (digitalRead(moveSns) == 0 && switchState != 0) {
      bndl.add("/moveSns").add(0);
      bundle_send(&bndl);
      switchState = 0;
    }
  }
}




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

void bundle_send(OSCBundle* bndl) {
  Udp.beginPacket(pcIP, pcPort);  //remotePort);
  bndl->send(Udp);
  Udp.endPacket();
  bndl->empty();
}


// WOL
void send_wol_packet() {
  udp_client.begin(7);
  udp_client.beginPacket(broadcast_ip, 7);
  udp_client.write(first_six_bytes, sizeof first_six_bytes);
  for (int i = 0; i < 16; i++) {
    udp_client.write(target_mac, sizeof target_mac);
  }
  udp_client.endPacket();
  udp_client.stop();
  Serial.println("send WOL");
}


// packetInHandler
void packedInHandler() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    OSCBundle bundleIn;
    for (int i = 0; i < packetSize; i++) {
      bundleIn.fill(packetBuffer[i]);
    }
    int bundleInSize = bundleIn.size();
    if (!bundleIn.hasError()) {
      bundleIn.route("/maxSystemState", maxSystemState);
      bundleIn.route("/version", versionHandler);
    }
    bundleIn.empty();
  }
}


void maxSystemState(OSCMessage& msg, int addrOffset) {
  int maxSystemState;
  maxSystemState = msg.getInt(0);
  switch (maxSystemState) {
    case 0:
      // max si chiude e con esso il sistema operativo
      digitalWrite(redLed, LOW);
      delay(10000);
      digitalWrite(onOffRelLed, LOW);
      delay(10000);
      systemStatus = 0;
      digitalWrite(onOffRel, LOW);
      break;
    case 1:
      // max acceso, chiudo contatti  ampificatore e setto system status 1
      digitalWrite(ampRel, HIGH);
      digitalWrite(ampRelLed, HIGH);
      delay(1000);
      systemStatus = 1;
      digitalWrite(redLed, LOW);
      digitalWrite(grnLed, HIGH);
      break;
  }
}


void versionHandler(OSCMessage& msg, int addrOffset) {
  int id = -1;
  if (msg.size() == 1) id = msg.getInt(0);
  bndl.add("/version/61").add(Version).add(id);
  bundle_send(&bndl);
}

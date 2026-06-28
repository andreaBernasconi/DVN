/*
Title: DVNisle
Board: OLIMEX ESP32‑S3‑DevKit‑LiPo
USB CDC On Boot → Enabled
USB Mode → Hardware CDC and JTAG
Flash Size → 8MB
Flash Mode → QIO
Flash Frequency → 80 MHz
Partition Scheme → Default 8MB with spiffs
PSRAM → OPI PSRAM
CPU Frequency → 240 MHz
Upload Speed: 921600
*/

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <OSCMessage.h>
#include <Logging.h>
#include <OscUtils.h>
#include <Utils.h>
#include <RadioConfig.h>
#include <esp32core.h>

#include "pwmLed.h"
#include "tlcLed.h"

// -----------------------------------------------------------------------------
// PWM CONFIGURATION
// -----------------------------------------------------------------------------

int pwmPins[] = {4, 7, 15, 16, 17, 18};
const int pwmCount = 6;

#define PWM_NUM_RGB    0
#define PWM_NUM_SINGLE 6

// -----------------------------------------------------------------------------
// TLC CONFIGURATION
// -----------------------------------------------------------------------------

#define TLC_NUM_RGB 8
#define TLC_NUM_LED 0

#define TLC_CLK 12
#define TLC_MOSI 11

// -----------------------------------------------------------------------------
// OSC ROUTER 
// -----------------------------------------------------------------------------

void oscRouter(OSCMessage &msg) {
    tlcOscRouter(msg);
    pwmOscRouter(msg);
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------

void setup() {
    initCommon();

    // PWM engine
    pwmLedInit(pwmPins, pwmCount);

    pwmLedSetErrorCallback([](const char* code){
        sendOscError("/error/pwmLed", code);
    });

    pwmLedConfigure(PWM_NUM_RGB, PWM_NUM_SINGLE);
    pwmLedInitMapping();

    // TLC engine
    tlcLedConfigure(TLC_NUM_RGB, TLC_NUM_LED);
    tlcLedInitMapping();
    tlcLedInitHardware(TLC_CLK, TLC_MOSI);

    tlcLedSetErrorCallback([](const char* code){
        sendOscError("/error/tlc", code);
    });

    // Unified OSC router
    setOscCallback(oscRouter);
}

// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------

void loop() {
    pwmLedUpdate();
    tlcLedUpdate();
}

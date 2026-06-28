/*
Title: ESP32 DVNdspl
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

// -----------------------------------------------------------------------------
// USER CONFIGURATION
// -----------------------------------------------------------------------------

int pwmPins[] = {4, 7, 15};
const int pwmCount = 3;

#define PWM_NUM_RGB    1
#define PWM_NUM_SINGLE 0

// -----------------------------------------------------------------------------
// OSC ROUTER
// -----------------------------------------------------------------------------

void oscRouter(OSCMessage &msg) {
    pwmOscRouter(msg);
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------

void setup() {
    initCommon();

    // PWM engine
    pwmLedInit(pwmPins, pwmCount);

    // Error callback using sendOscError()
    pwmLedSetErrorCallback([](const char* code){
        sendOscError("/error/pwmLed", code);
    });

    pwmLedConfigure(PWM_NUM_RGB, PWM_NUM_SINGLE);
    pwmLedInitMapping();

    // OSC router
    setOscCallback(oscRouter);
}

// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------

void loop() {
    pwmLedUpdate();
}

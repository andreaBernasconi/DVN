# DVNisle — LED Node (ESP32‑S3)

DVNisle is a firmware in the DVN family designed to control:

- Multiple PWM LEDs
- One or two TLC59711 LED drivers (12 or 24 channels)
- OSC communication over ESP‑NOW

It is built on top of esp32core, the unified PWM engine, and the TLC engine.

---

## Features

- PWM engine for single LEDs and RGB groups
- TLC engine for 12/24‑channel LED control
- Smooth fades with gamma correction
- Unified OSC routing for PWM and TLC
- Unified error reporting (sendOscError())
- Clean and modular architecture

---

## Indexing Note (Important)

OSC commands use **1‑based indexing**  
Internal firmware functions use **0‑based indexing**

Example:  
- OSC: `/pwmLed 1 ...` → first LED  
- Internal: `pwmLed(0, ...)` → first LED  

The OSC router automatically converts indices.

---

## Hardware

Board: OLIMEX ESP32‑S3‑DevKit‑LiPo

Supported LED engines:
- PWM pins (configurable)
- TLC59711 via SPI (CI/DI)

---

## OSC Communication (via ESP‑NOW)

DVNisle receives OSC messages through ESP‑NOW.
The firmware sets its OSC router with:

setOscCallback(oscRouter);

The router dispatches commands to:
- pwmOscRouter()
- tlcOscRouter()
- custom handlers

---

## OSC Commands

### /pwmLed index value time
Controls a single PWM LED.

Example:
/pwmLed 1 2048 500

### /pwmRgb index r g b time
Controls a logical RGB LED.

Example:
/pwmRgb 1 4095 0 0 800

### /tlcLed index value time
Controls a single TLC channel.

### /tlcRgb index r g b time
Controls an RGB group mapped to TLC channels.

---

## Creating Custom OSC Callbacks

You can add your own OSC commands by defining a handler function and registering it inside the OSC router.

### 1. Create a handler function

void handleCustom(OSCMessage &msg) {
    if (msg.size() < 1) {
        sendOscError("/error/dvn", "CUSTOM_ARGS_MISSING");
        return;
    }

    int value = msg.getInt(0);
    pwmLed(0, value, 300);
}

### 2. Register the handler in the OSC router

Inside oscRouter():

msg.dispatch("/dvn/custom", handleCustom);

### 3. Send the OSC message

/dvn/custom 2048

---

## Internal Functions (Firmware‑Callable)

### PWM engine

pwmLed(ledIndex, value, timeMs)  
pwmRgb(rgbIndex, r, g, b, timeMs)  
pwmLedUpdate()

### TLC engine

tlcLedFade(ch, value, timeMs)  
tlcRgbFade(rgbIndex, r, g, b, timeMs)  
tlcUpdate()

---

## Error Reporting

DVNisle can send OSC error messages from any internal function, including custom ones.

Use:
sendOscError("/error/pwmLed", "ERROR_CODE");

Examples:
sendOscError("/error/pwmLed", "PWM_INDEX_OUT_OF_RANGE");
sendOscError("/error/tlc", "TLC_INVALID_ARGS");
sendOscError("/error/dvn", "CUSTOM_ERROR");

Errors are automatically sent back to the last OSC sender via ESP‑NOW.

---

## End of file

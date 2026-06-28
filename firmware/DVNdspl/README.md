# DVNdspl — Display Node (ESP32‑S3)

DVNdspl is a lightweight firmware in the DVN family, designed to control:

- 1 RGB LED using 3 PWM channels
- OSC communication over ESP‑NOW

It is built on top of esp32core and the unified PWM engine, following the same architecture as all DVN firmwares.

---

## Features

- 3 PWM channels mapped to 1 logical RGB LED
- Smooth fades with gamma correction
- OSC control via ESP‑NOW
- Unified error reporting (sendOscError())
- Clean and modular architecture

---

## Indexing Note (Important)

OSC commands use **1‑based indexing**  
Internal firmware functions use **0‑based indexing**

Example:  
- OSC: `/pwmRgb 1 ...` → first RGB  
- Internal: `pwmRgb(0, ...)` → first RGB  

The OSC router automatically converts indices.

---

## Hardware

Board: OLIMEX ESP32‑S3‑DevKit‑LiPo

---

## OSC Communication (via ESP‑NOW)

DVNdspl receives OSC messages through ESP‑NOW.
The firmware sets its OSC router with:

setOscCallback(oscRouter);

---

## OSC Commands

### /pwmRgb index r g b time

Controls the logical RGB LED.

- index → always 1 in DVNdspl
- r, g, b → 0–4095
- time → fade duration in ms

Example:
/pwmRgb 1 4095 0 0 800

---

## Creating Custom OSC Callbacks

You can add your own OSC commands by defining a handler function and registering it inside the OSC router.

### 1. Create a handler function

void handleCustomColor(OSCMessage &msg) {
    if (msg.size() < 3) {
        sendOscError("/error/dvn", "CUSTOM_ARGS_MISSING");
        return;
    }

    uint16_t r = msg.getInt(0);
    uint16_t g = msg.getInt(1);
    uint16_t b = msg.getInt(2);

    pwmRgb(0, r, g, b, 500);
}

### 2. Register the handler in the OSC router

Inside oscRouter():

msg.dispatch("/dvn/customColor", handleCustomColor);

### 3. Send the OSC message

/dvn/customColor 4095 0 2048

---

## Internal Functions (Firmware‑Callable)

### pwmRgb(rgbIndex, r, g, b, timeMs)

Sets the logical RGB LED.

Example:
pwmRgb(0, 4095, 0, 0, 1000);

### pwmLedUpdate()

Updates the fade engine.
Must be called in the main loop.

---

## Error Reporting

DVNdspl can send OSC error messages from any internal function, including custom ones.

Use:
sendOscError("/error/pwmLed", "ERROR_CODE");

Examples:
sendOscError("/error/pwmLed", "RGB_VALUE_OUT_OF_RANGE");
sendOscError("/error/dvn", "NEOPIXEL_INIT_FAILED");

Errors are automatically sent back to the last OSC sender via ESP‑NOW.

---

## End of file

#pragma once

#include <Arduino.h>

class Debugger {
public:
    static void begin() {
        static bool debugger_initialized = false;
        if (debugger_initialized) return;

        Serial.begin(115200);
        debugger_initialized = true;

        auto start_t = millis();
        while (!Serial && millis() - start_t < 15000ul) delay(100);

        delay(2000);
    }
};

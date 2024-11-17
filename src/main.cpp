#include "constants.h"
#include "misc/button_manager.h"
#include "misc/state_machine.h"

#include <lib/misc/led.h>
#include <lib/misc/vector.h>
#include <lib/network/now_io.h>
#include <misc/debugger.h>

StateMachine state_machine;
Led led(LED_PIN);
ButtonManager<BUTTON_COUNT> button_manager(BUTTON_PINS);

void setup() {
    VERBOSE(Debugger::begin());

    button_manager.begin();

    led.set_blink_repeat_interval(LED_HOLDING_BLINK_INTERVAL);
    led.begin();

    led.set_brightness(0xA0);
    led.flash();
}

void loop() {
    state_machine.execute();
    button_manager.tick();
    led.tick();

    delay(DELAY_AMOUNT);
}

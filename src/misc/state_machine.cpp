#include "state_machine.h"

#include <lib/misc/led.h>
#include <lib/network/now_io.h>

#include "misc/async_handlers/button_event_send_handler.h"
#include "misc/async_handlers/discovery_handler.h"
#include "misc/async_handlers/state_indication_handler.h"
#include "misc/debugger.h"
#include "misc/button_manager.h"
#include "constants.h"

RTC_DATA_ATTR bool hub_addr_present = false;
RTC_DATA_ATTR uint8_t hub_addr[6] {};
RTC_DATA_ATTR uint8_t wifi_channel = 0;
RTC_DATA_ATTR uint8_t error_count = 0;

uint32_t initialized_time = 0;

DiscoveryHandler discovery_handler;
ButtonEventSendHandler button_event_sender;
StateIndicationHandler state_indication_handler;

extern Led led;
extern ButtonManager<BUTTON_COUNT> button_manager;

void StateMachine::execute() {
    ApplicationState prev_state;
    do {
        prev_state = _state;
        _execute();
    } while (prev_state != _state);
}

void StateMachine::_change_state(ApplicationState new_state) {
    if (_state == new_state) return;

    VERBOSE(D_PRINTF("StateMachine: Change state from %i to %i\r\n", _state, new_state));
    _state = new_state;
}

void StateMachine::_execute() {
    switch (_state) {
        case ApplicationState::INITIAL:
            return _initial();

        case ApplicationState::RESET:
            return _reset();

        case ApplicationState::NETWORK_INITIALIZATION:
            return _network_initialization();

        case ApplicationState::DISCOVERY:
            return _discovery();

        case ApplicationState::DISCOVERY_WAIT:
            return _discovery_wait();

        case ApplicationState::BUTTON_HANDLE:
            return _button_handle();

        case ApplicationState::DATA_SENDING:
            return _data_sending();

        case ApplicationState::DATA_SENDING_WAIT:
            return _data_sending_wait();

        case ApplicationState::FINISHED:
            return _finished();

        case ApplicationState::RESULT_INDICATION:
            return _result_indication();

        case ApplicationState::RESULT_INDICATION_WAIT:
            return _result_indication_wait();

        case ApplicationState::TURNING_OFF:
            return _turning_off();

        case ApplicationState::END:
            return _end();

        default:
            D_PRINTF("StateMachine: Unknown state %i\r\n", _state);
            _change_state(ApplicationState::END);
    }
}

void StateMachine::_initial() {
    if (error_count >= SEND_ERROR_BEFORE_RESET) {
        D_PRINT("Too many sending errors. Resetting saved HUB...");
        _change_state(ApplicationState::RESET);
    } else {
        _change_state(ApplicationState::NETWORK_INITIALIZATION);
    }
}

void StateMachine::_reset() {
    hub_addr_present = false;
    memset(hub_addr, 0, sizeof(hub_addr));
    wifi_channel = 0;
    error_count = 0;

    _change_state(ApplicationState::NETWORK_INITIALIZATION);
}

void StateMachine::_network_initialization() {
#ifdef DEBUG
    Debugger::begin();
#endif

    initialized_time = millis();

    NowIo::instance().begin();
    AsyncEspNowInteraction::print_mac();

    if (hub_addr_present) {
        AsyncEspNow::instance().change_channel(wifi_channel);
        _change_state(ApplicationState::BUTTON_HANDLE);
    } else {
        _change_state(ApplicationState::DISCOVERY);
    }
}

void StateMachine::_discovery() {
    discovery_handler.discover();
    _change_state(ApplicationState::DISCOVERY_WAIT);
}

void StateMachine::_discovery_wait() {
    if (discovery_handler.state() == DiscoveryHandler::State::PENDING) return;

    if (discovery_handler.state() != DiscoveryHandler::State::SUCCESS) {
        D_PRINT("*** Unable to find HUB. Exit...");

        _change_state(ApplicationState::FINISHED);
        _command_state = CommandState::HUB_MISSING;
        return;
    }

    hub_addr_present = true;
    wifi_channel = discovery_handler.hub_channel();
    memcpy(hub_addr, discovery_handler.hub_mac_addr(), sizeof(hub_addr));

    _change_state(ApplicationState::BUTTON_HANDLE);
}

void StateMachine::_button_handle() {
    if (button_manager.idle() && millis() - initialized_time > BUTTON_WAIT_TIMEOUT) {
        _change_state(ApplicationState::DATA_SENDING);
    }
}

void StateMachine::_data_sending() {
    if (button_manager.empty()) {
        D_PRINT("Nothing to send");

        _change_state(ApplicationState::FINISHED);
        _command_state = CommandState::NOTHING_TO_SEND;
        return;
    }

    button_event_sender.send(hub_addr, button_manager.events());
    _change_state(ApplicationState::DATA_SENDING_WAIT);
}

void StateMachine::_data_sending_wait() {
    using State = ButtonEventSendHandler::State;
    auto state = button_event_sender.state();

    if (state == State::PENDING) return;

    if (state == State::SUCCESS) {
        _command_state = CommandState::SUCCESS;
        _change_state(ApplicationState::FINISHED);
        error_count = 0;
    } else {
        D_PRINTF("Failed to send message: %s\r\n", state == State::ERROR ? "error" : "timeout");
        _command_state = state == State::ERROR ? CommandState::SEND_ERROR : CommandState::SEND_TIMEOUT;
        _change_state(ApplicationState::FINISHED);
        ++error_count;
    }
}

void StateMachine::_finished() {
    if (button_manager.active()) return;

    button_manager.end();
    _change_state(ApplicationState::RESULT_INDICATION);
}

void StateMachine::_result_indication() {
    bool need_indication = false;
    uint8_t blink_count = 0;
    switch (_command_state) {
        case CommandState::HUB_MISSING:
            blink_count = 5;
            need_indication = true;
            break;
        case CommandState::SEND_TIMEOUT:
            blink_count = 4;
            need_indication = true;
            break;
        case CommandState::SEND_ERROR:
            blink_count = 3;
            need_indication = true;
            break;

        default:;
    }

    if (need_indication) {
        state_indication_handler.start(led, blink_count);
        _change_state(ApplicationState::RESULT_INDICATION_WAIT);
    } else {
        _change_state(ApplicationState::TURNING_OFF);
    }
}

void StateMachine::_result_indication_wait() {
    if (state_indication_handler.state() == StateIndicationHandler::State::PENDING) return;

    _change_state(ApplicationState::TURNING_OFF);
}

void StateMachine::_turning_off() {
    led.turn_off();

    D_PRINTF("Finished with result: %u\r\n", _command_state);
    _change_state(ApplicationState::END);

    uint64_t button_mask = 0;
    for (int i = 0; i < BUTTON_COUNT; ++i) button_mask |= 1 << BUTTON_PINS[i];

    esp_deep_sleep_enable_gpio_wakeup(button_mask, ESP_GPIO_WAKEUP_GPIO_HIGH);
    esp_deep_sleep_start();
}

void StateMachine::_end() {
    D_PRINT("You shouldn't be here 0_0");
    esp_restart();
}

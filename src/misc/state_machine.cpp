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

uint32_t button_wait_start_time = 0;
uint16_t button_event_sent_count = 0;
uint16_t send_retry_count = 0;

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

    if (new_state == ApplicationState::BUTTON_HANDLE) {
        button_wait_start_time = millis();
    }

    _state = new_state;
}

void StateMachine::_execute() {
    // @formatter:off
    switch (_state) {
        case ApplicationState::INITIAL:                     _initial(); break;
        case ApplicationState::RESET:                       _reset(); break;
        case ApplicationState::NETWORK_INITIALIZATION:      _network_initialization(); break;
        case ApplicationState::DISCOVERY:                   _discovery(); break;
        case ApplicationState::DISCOVERY_WAIT:              _discovery_wait(); break;
        case ApplicationState::BUTTON_HANDLE:               _button_handle(); break;
        case ApplicationState::DATA_SENDING:                _data_sending(); break;
        case ApplicationState::DATA_SENDING_WAIT:           _data_sending_wait(); break;
        case ApplicationState::DATA_SENDING_SUCCESS:        _data_sending_success(); break;
        case ApplicationState::DATA_SENDING_ERROR:          _data_sending_error(); break;
        case ApplicationState::FINISHED:                    _finished(); break;
        case ApplicationState::RESULT_INDICATION:           _result_indication(); break;
        case ApplicationState::RESULT_INDICATION_WAIT:      _result_indication_wait(); break;
        case ApplicationState::TURNING_OFF:                 _turning_off(); break;
        case ApplicationState::END:                         _end(); break;

        default:
            D_PRINTF("StateMachine: Unknown state %i\r\n", _state);
            _change_state(ApplicationState::END);
    }
    // @formatter:on
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
    auto can_continue = button_event_sent_count == 0
                            ? ((button_manager.holding() || button_manager.idle())
                                && millis() - button_wait_start_time > BUTTON_WAIT_TIMEOUT)
                            : millis() - button_wait_start_time > BUTTON_REPEAT_TIMEOUT;

    if (can_continue) _change_state(ApplicationState::DATA_SENDING);
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
    auto state = button_event_sender.state();
    if (state == ButtonEventSendHandler::State::PENDING) return;

    if (state == ButtonEventSendHandler::State::SUCCESS) {
        _change_state(ApplicationState::DATA_SENDING_SUCCESS);
    } else {
        _change_state(ApplicationState::DATA_SENDING_ERROR);
    }
}

void StateMachine::_data_sending_success() {
    if (button_manager.holding()) {
        D_PRINT("Button still pressed. Repeating...");
        ++button_event_sent_count;
        _change_state(ApplicationState::BUTTON_HANDLE);
    } else if (button_event_sent_count > 0) {
        D_PRINT("Button released. Sending release event...");
        button_event_sent_count = 0;
        _change_state(ApplicationState::DATA_SENDING);
    } else {
        _command_state = CommandState::SUCCESS;
        _change_state(ApplicationState::FINISHED);
    }

    error_count = 0;
}

void StateMachine::_data_sending_error() {
    using State = ButtonEventSendHandler::State;
    auto state = button_event_sender.state();

    if (send_retry_count < SEND_RETRY_COUNT) {
        D_PRINT("Data sending failed. Retrying...");
        _change_state(ApplicationState::DATA_SENDING);
        ++send_retry_count;
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
    bool need_indication = true;
    uint8_t blink_count = 0;
    // @formatter:off
    switch (_command_state) {
        case CommandState::HUB_MISSING:      blink_count = 5; break;
        case CommandState::SEND_TIMEOUT:     blink_count = 4; break;
        case CommandState::SEND_ERROR:       blink_count = 3; break;
        default: need_indication = false;
    }
    // @formatter:on

    if (need_indication) {
        state_indication_handler.blink(led, blink_count);
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

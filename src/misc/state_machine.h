#pragma once

#include <cstdint>

enum class ApplicationState: uint8_t {
    INITIAL,

    RESET,

    NETWORK_INITIALIZATION,
    DISCOVERY,
    DISCOVERY_WAIT,

    BUTTON_HANDLE,
    DATA_SENDING,

    FINISHED,
    RESULT_INDICATION,

    TURNING_OFF,
    END
};

enum class CommandState: uint8_t {
    UNKNOWN,

    SUCCESS,
    NOTHING_TO_SEND,

    HUB_MISSING,
    SEND_TIMEOUT,
    SEND_ERROR
};

class StateMachine {
public:
    void execute();

private:
    ApplicationState _state = ApplicationState::INITIAL;
    CommandState _command_state = CommandState::UNKNOWN;

    void _change_state(ApplicationState new_state);
    void _execute();

    void _initial();
    void _reset();
    void _network_initialization();
    void _discovery();
    void _discovery_wait();
    void _button_handle();
    void _data_sending();
    void _finished();
    void _result_indication();
    void _turning_off();
    void _end();
};

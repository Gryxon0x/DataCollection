#include <app/app_state.h>

const char *app_state_to_string(enum app_state state)
{
    switch (state) {
    case APP_STATE_IDLE:
        return "IDLE";
    case APP_STATE_COLLECTING:
        return "COLLECTING";
    case APP_STATE_TRANSMITTING:
        return "TRANSMITTING";
    case APP_STATE_DONE:
        return "DONE";
    case APP_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}
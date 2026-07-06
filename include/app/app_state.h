#ifndef APP_STATE_H
#define APP_STATE_H

enum app_state {
    APP_STATE_IDLE = 0,
    APP_STATE_COLLECTING,
    APP_STATE_TRANSMITTING,
    APP_STATE_DONE,
    APP_STATE_ERROR
};

const char *app_state_to_string(enum app_state state);

#endif
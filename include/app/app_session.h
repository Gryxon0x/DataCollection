#ifndef APP_SESSION_H
#define APP_SESSION_H

#include <stdint.h>

#include <app/app_context.h>

void app_session_clear(struct app_context *ctx);
int app_session_collect(struct app_context *ctx, uint32_t duration_ms);
int app_session_transmit_raw(struct app_context *ctx);

#endif
#include <app/app_commands.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <zephyr/sys/printk.h>

#include <app/app_state.h>
#include <transport/ble_data_service.h>

static struct app_context *g_ctx;

static void send_status(void)
{
    char line[128];

    snprintf(line, sizeof(line),
             "STATUS,%s,samples=%u,max=%u,pos=%d,mode=%d,alg=%d\n",
             app_state_to_string(g_ctx->state),
             g_ctx->sample_count,
             MAX_SAMPLES,
             g_ctx->position,
             g_ctx->strategy,
             g_ctx->algorithm_id);

    ble_data_service_send_text(line);
}

void app_commands_init(struct app_context *ctx)
{
    g_ctx = ctx;
}

void app_command_handler(const char *command)
{
    printk("App command handler: %s\n", command);

    if (g_ctx == NULL) {
        ble_data_service_send_text("ERROR,APP_CONTEXT_NULL\n");
        return;
    }

    if (strcmp(command, "PING") == 0) {
        ble_data_service_send_text("PONG\n");
        return;
    }

    if (strncmp(command, "START,", 6) == 0) {
        uint32_t duration = (uint32_t)atoi(&command[6]);

        if (duration == 0) {
            ble_data_service_send_text("ERROR,INVALID_DURATION\n");
            return;
        }

        uint32_t max_duration = MAX_SAMPLES * SAMPLE_PERIOD_MS;

        if (duration > max_duration) {
            duration = max_duration;
        }

        if (g_ctx->state == APP_STATE_COLLECTING ||
            g_ctx->state == APP_STATE_TRANSMITTING) {
            ble_data_service_send_text("ERROR,BUSY\n");
            return;
        }

        g_ctx->requested_duration_ms = duration;
        g_ctx->start_requested = true;

        ble_data_service_send_text("START_ACCEPTED\n");
        return;
    }

    if (strcmp(command, "SEND") == 0) {
        if (g_ctx->state == APP_STATE_COLLECTING ||
            g_ctx->state == APP_STATE_TRANSMITTING) {
            ble_data_service_send_text("ERROR,BUSY\n");
            return;
        }

        g_ctx->send_requested = true;
        ble_data_service_send_text("SEND_ACCEPTED\n");
        return;
    }

    if (strcmp(command, "CLEAR") == 0) {
        if (g_ctx->state == APP_STATE_COLLECTING ||
            g_ctx->state == APP_STATE_TRANSMITTING) {
            ble_data_service_send_text("ERROR,BUSY\n");
            return;
        }

        g_ctx->clear_requested = true;
        ble_data_service_send_text("CLEAR_ACCEPTED\n");
        return;
    }

    if (strcmp(command, "STATUS") == 0) {
        g_ctx->status_requested = true;
        return;
    }

    ble_data_service_send_text("ERROR,UNKNOWN_COMMAND\n");
}
#include <app/app.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <sensor/bma400_app.h>
#include <transport/ble_data_service.h>

#include <app/app_context.h>
#include <app/app_commands.h>
#include <app/app_session.h>
#include <app/app_state.h>

static struct app_context app_ctx = {
    .state = APP_STATE_IDLE,
    .sample_count = 0,

    .start_requested = false,
    .send_requested = false,
    .clear_requested = false,
    .status_requested = false,

    .requested_duration_ms = 0,

    .position = SENSOR_POSITION_WRIST,
    .strategy = PROCESSING_RAW,
    .algorithm_id = STEP_ALG_NONE,
};

static void app_send_status(void)
{
    char line[128];

    snprintf(line, sizeof(line),
             "STATUS,%s,samples=%u,max=%u,pos=%d,mode=%d,alg=%d\n",
             app_state_to_string(app_ctx.state),
             app_ctx.sample_count,
             MAX_SAMPLES,
             app_ctx.position,
             app_ctx.strategy,
             app_ctx.algorithm_id);

    ble_data_service_send_text(line);
}

int app_init(void)
{
    int ret;

    ret = bma400_app_init();
    if (ret != 0) {
        printk("BMA400 init failed, ret=%d\n", ret);
        return ret;
    }

    printk("BMA400 init OK\n");

    ret = ble_data_service_init();
    printk("ble_data_service_init ret=%d\n", ret);

    if (ret != 0) {
        printk("BLE init failed, ret=%d\n", ret);
        return ret;
    }

    app_commands_init(&app_ctx);
    ble_data_service_set_command_handler(app_command_handler);

    printk("Ready. Commands: PING, STATUS, CLEAR, SEND, START,<duration_ms>\n");

    return 0;
}

void app_process(void)
{
    if (app_ctx.status_requested) {
        app_ctx.status_requested = false;
        app_send_status();
    }

    if (app_ctx.clear_requested) {
        app_ctx.clear_requested = false;
        app_session_clear(&app_ctx);
        ble_data_service_send_text("CLEARED\n");
    }

    if (app_ctx.start_requested) {
        uint32_t duration = app_ctx.requested_duration_ms;

        app_ctx.start_requested = false;

        app_session_collect(&app_ctx, duration);

        if (app_ctx.state == APP_STATE_DONE) {
            app_session_transmit_raw(&app_ctx);
        }
    }

    if (app_ctx.send_requested) {
        app_ctx.send_requested = false;
        app_session_transmit_raw(&app_ctx);
    }

    printk("Heartbeat: state=%s connected=%d notify=%d samples=%u\n",
           app_state_to_string(app_ctx.state),
           ble_data_service_is_connected(),
           ble_data_service_notifications_enabled(),
           app_ctx.sample_count);
}
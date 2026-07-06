#include <app/app_session.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>

#include <sensor/bma400_app.h>
#include <transport/ble_data_service.h>
#include <transport/ble_protocol.h>

void app_session_clear(struct app_context *ctx)
{
    ctx->sample_count = 0;
    ctx->state = APP_STATE_IDLE;
}

int app_session_transmit_raw(struct app_context *ctx)
{
    uint8_t packet[20];
    int ret;

    if (!ble_data_service_is_connected()) {
        printk("Cannot transmit: BLE not connected\n");
        return -1;
    }

    if (!ble_data_service_notifications_enabled()) {
        printk("Cannot transmit: notifications not enabled\n");
        return -1;
    }

    ctx->state = APP_STATE_TRANSMITTING;

    printk("Transmitting %u binary samples\n", ctx->sample_count);

    packet[0] = BLE_PKT_BEGIN;
    sys_put_le32(ctx->sample_count, &packet[1]);
    sys_put_le16(SAMPLE_PERIOD_MS, &packet[5]);

    ret = ble_data_service_send_bytes(packet, 7);
    if (ret != 0) {
        printk("BLE BEGIN send failed: %d\n", ret);
        ctx->state = APP_STATE_ERROR;
        return ret;
    }

    for (uint32_t i = 0; i < ctx->sample_count; i++) {
        packet[0] = BLE_PKT_SAMPLE;
        sys_put_le32(ctx->samples[i].sample_id, &packet[1]);
        sys_put_le32(ctx->samples[i].t_ms, &packet[5]);
        sys_put_le16((uint16_t)ctx->samples[i].ax, &packet[9]);
        sys_put_le16((uint16_t)ctx->samples[i].ay, &packet[11]);
        sys_put_le16((uint16_t)ctx->samples[i].az, &packet[13]);

        ret = ble_data_service_send_bytes(packet, 15);
        if (ret != 0) {
            printk("BLE SAMPLE send failed at sample %u, ret=%d\n", i, ret);
            ctx->state = APP_STATE_ERROR;
            return ret;
        }
    }

    packet[0] = BLE_PKT_END;
    sys_put_le32(ctx->sample_count, &packet[1]);

    ret = ble_data_service_send_bytes(packet, 5);
    if (ret != 0) {
        printk("BLE END send failed: %d\n", ret);
        ctx->state = APP_STATE_ERROR;
        return ret;
    }

    ctx->state = APP_STATE_DONE;

    printk("Binary transmission done\n");

    return 0;
}

int app_session_collect(struct app_context *ctx, uint32_t duration_ms)
{
    int ret;
    int64_t t0_ms;
    int64_t next_sample_ms;
    uint32_t max_requested_samples;

    ctx->state = APP_STATE_COLLECTING;
    ctx->sample_count = 0;

    max_requested_samples = duration_ms / SAMPLE_PERIOD_MS;

    if (max_requested_samples > MAX_SAMPLES) {
        max_requested_samples = MAX_SAMPLES;
    }

    printk("Collecting samples: duration=%u ms, target=%u samples\n",
           duration_ms,
           max_requested_samples);

    ble_data_service_send_text("COLLECTING\n");

    t0_ms = k_uptime_get();
    next_sample_ms = t0_ms;

    for (uint32_t i = 0; i < max_requested_samples; i++) {
        struct bma400_app_accel_raw accel;
        int64_t now_ms;
        int64_t sleep_ms;

        now_ms = k_uptime_get();
        sleep_ms = next_sample_ms - now_ms;

        if (sleep_ms > 0) {
            k_sleep(K_MSEC((uint32_t)sleep_ms));
        }

        now_ms = k_uptime_get();

        ret = bma400_app_read_accel_raw(&accel);
        if (ret != 0) {
            printk("bma400_app_read_accel_raw failed: %d\n", ret);
            ctx->state = APP_STATE_ERROR;
            ble_data_service_send_text("ERROR,BMA400_READ_FAILED\n");
            return ret;
        }

        ctx->samples[ctx->sample_count].sample_id = ctx->sample_count;
        ctx->samples[ctx->sample_count].t_ms = (uint32_t)(now_ms - t0_ms);
        ctx->samples[ctx->sample_count].ax = accel.x;
        ctx->samples[ctx->sample_count].ay = accel.y;
        ctx->samples[ctx->sample_count].az = accel.z;

        ctx->sample_count++;

        next_sample_ms += SAMPLE_PERIOD_MS;
    }

    ctx->state = APP_STATE_DONE;

    printk("Collection done: samples=%u\n", ctx->sample_count);
    ble_data_service_send_text("COLLECTION_DONE\n");

    return 0;
}
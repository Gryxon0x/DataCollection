#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>

#include "bma400_app.h"
#include "ble_data_service.h"

#define SAMPLE_PERIOD_MS 20
#define MAX_SAMPLES 1000

#define BLE_PKT_SAMPLE 0x01
#define BLE_PKT_BEGIN  0x10
#define BLE_PKT_END    0x11

struct accel_record {
    uint32_t sample_id;
    uint32_t t_ms;
    int16_t ax;
    int16_t ay;
    int16_t az;
};

enum app_state {
    APP_STATE_IDLE = 0,
    APP_STATE_COLLECTING,
    APP_STATE_TRANSMITTING,
    APP_STATE_DONE,
    APP_STATE_ERROR
};

static struct accel_record samples[MAX_SAMPLES];
static uint32_t sample_count;

static volatile bool start_requested;
static volatile bool send_requested;
static volatile bool clear_requested;
static volatile bool status_requested;

static uint32_t requested_duration_ms;

static enum app_state state = APP_STATE_IDLE;

static const char *state_to_string(enum app_state s)
{
    switch (s) {
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

static void send_status(void)
{
    char line[96];

    snprintf(line, sizeof(line),
             "STATUS,%s,samples=%u,max=%u\n",
             state_to_string(state),
             sample_count,
             MAX_SAMPLES);

    ble_data_service_send_text(line);
}

static void app_command_handler(const char *command)
{
    printk("App command handler: %s\n", command);

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

        if (state == APP_STATE_COLLECTING || state == APP_STATE_TRANSMITTING) {
            ble_data_service_send_text("ERROR,BUSY\n");
            return;
        }

        requested_duration_ms = duration;
        start_requested = true;

        ble_data_service_send_text("START_ACCEPTED\n");
        return;
    }

    if (strcmp(command, "SEND") == 0) {
        if (state == APP_STATE_COLLECTING || state == APP_STATE_TRANSMITTING) {
            ble_data_service_send_text("ERROR,BUSY\n");
            return;
        }

        send_requested = true;
        ble_data_service_send_text("SEND_ACCEPTED\n");
        return;
    }

    if (strcmp(command, "CLEAR") == 0) {
        if (state == APP_STATE_COLLECTING || state == APP_STATE_TRANSMITTING) {
            ble_data_service_send_text("ERROR,BUSY\n");
            return;
        }

        clear_requested = true;
        ble_data_service_send_text("CLEAR_ACCEPTED\n");
        return;
    }

    if (strcmp(command, "STATUS") == 0) {
        status_requested = true;
        return;
    }

    ble_data_service_send_text("ERROR,UNKNOWN_COMMAND\n");
}

static void collect_samples(uint32_t duration_ms)
{
    int ret;
    int64_t t0_ms;
    int64_t next_sample_ms;
    uint32_t max_requested_samples;

    state = APP_STATE_COLLECTING;
    sample_count = 0;

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
            state = APP_STATE_ERROR;
            ble_data_service_send_text("ERROR,BMA400_READ_FAILED\n");
            return;
        }

        samples[sample_count].sample_id = sample_count;
        samples[sample_count].t_ms = (uint32_t)(now_ms - t0_ms);
        samples[sample_count].ax = accel.x;
        samples[sample_count].ay = accel.y;
        samples[sample_count].az = accel.z;

        sample_count++;

        next_sample_ms += SAMPLE_PERIOD_MS;
    }

    state = APP_STATE_DONE;

    printk("Collection done: samples=%u\n", sample_count);
    ble_data_service_send_text("COLLECTION_DONE\n");
}

static void transmit_samples(void)
{
    uint8_t packet[20];

    if (!ble_data_service_is_connected()) {
        printk("Cannot transmit: BLE not connected\n");
        return;
    }

    if (!ble_data_service_notifications_enabled()) {
        printk("Cannot transmit: notifications not enabled\n");
        return;
    }

    state = APP_STATE_TRANSMITTING;

    printk("Transmitting %u binary samples\n", sample_count);

    /*
     * BEGIN packet:
     * [0]     type = 0x10
     * [1..4]  sample_count uint32 LE
     * [5..6]  sample_period_ms uint16 LE
     */
    packet[0] = BLE_PKT_BEGIN;
    sys_put_le32(sample_count, &packet[1]);
    sys_put_le16(SAMPLE_PERIOD_MS, &packet[5]);

    int ret = ble_data_service_send_bytes(packet, 7);
    if (ret != 0) {
        printk("BLE BEGIN send failed: %d\n", ret);
        state = APP_STATE_ERROR;
        return;
    }

    for (uint32_t i = 0; i < sample_count; i++) {
        /*
         * SAMPLE packet:
         * [0]      type = 0x01
         * [1..4]   sample_id uint32 LE
         * [5..8]   t_ms uint32 LE
         * [9..10]  ax int16 LE
         * [11..12] ay int16 LE
         * [13..14] az int16 LE
         */
        packet[0] = BLE_PKT_SAMPLE;
        sys_put_le32(samples[i].sample_id, &packet[1]);
        sys_put_le32(samples[i].t_ms, &packet[5]);
        sys_put_le16((uint16_t)samples[i].ax, &packet[9]);
        sys_put_le16((uint16_t)samples[i].ay, &packet[11]);
        sys_put_le16((uint16_t)samples[i].az, &packet[13]);

        ret = ble_data_service_send_bytes(packet, 15);
        if (ret != 0) {
            printk("BLE SAMPLE send failed at sample %u, ret=%d\n", i, ret);
            state = APP_STATE_ERROR;
            return;
        }
    }

    /*
     * END packet:
     * [0]     type = 0x11
     * [1..4]  sample_count uint32 LE
     */
    packet[0] = BLE_PKT_END;
    sys_put_le32(sample_count, &packet[1]);

    ret = ble_data_service_send_bytes(packet, 5);
    if (ret != 0) {
        printk("BLE END send failed: %d\n", ret);
        state = APP_STATE_ERROR;
        return;
    }

    state = APP_STATE_DONE;

    printk("Binary transmission done\n");
}

int main(void)
{
    int ret;

    k_msleep(3000);

    printk("\n\n=== BMA400 BLE data collector boot ===\n");

    ret = bma400_app_init();
    if (ret != 0) {
        while (1) {
            printk("BMA400 init failed, ret=%d\n", ret);
            k_sleep(K_MSEC(1000));
        }
    }

    printk("BMA400 init OK\n");

    ret = ble_data_service_init();
    printk("ble_data_service_init ret=%d\n", ret);

    if (ret != 0) {
        while (1) {
            printk("BLE init failed, ret=%d\n", ret);
            k_sleep(K_MSEC(1000));
        }
    }

    ble_data_service_set_command_handler(app_command_handler);

    printk("Ready. Commands: PING, STATUS, CLEAR, SEND, START,<duration_ms>\n");

    while (1) {
        if (status_requested) {
            status_requested = false;
            send_status();
        }

        if (clear_requested) {
            clear_requested = false;
            sample_count = 0;
            state = APP_STATE_IDLE;
            ble_data_service_send_text("CLEARED\n");
        }

        if (start_requested) {
            uint32_t duration = requested_duration_ms;

            start_requested = false;

            collect_samples(duration);

            if (state == APP_STATE_DONE) {
                transmit_samples();
            }
        }

        if (send_requested) {
            send_requested = false;
            transmit_samples();
        }

        printk("Heartbeat: state=%s connected=%d notify=%d samples=%u\n",
               state_to_string(state),
               ble_data_service_is_connected(),
               ble_data_service_notifications_enabled(),
               sample_count);

        k_sleep(K_MSEC(500));
    }

    return 0;
}
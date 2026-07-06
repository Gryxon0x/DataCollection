#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include <stdint.h>
#include <stdbool.h>

#include <app/app_state.h>
#include <step/step_types.h>

#define SAMPLE_PERIOD_MS 20
#define MAX_SAMPLES 1000

struct app_context {
    enum app_state state;

    struct accel_record samples[MAX_SAMPLES];
    uint32_t sample_count;

    volatile bool start_requested;
    volatile bool send_requested;
    volatile bool clear_requested;
    volatile bool status_requested;

    uint32_t requested_duration_ms;

    enum sensor_position position;
    enum processing_strategy strategy;
    enum step_algorithm_id algorithm_id;
};

#endif
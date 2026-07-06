#ifndef STEP_TYPES_H
#define STEP_TYPES_H

#include <stdint.h>

enum sensor_position {
    SENSOR_POSITION_WRIST = 0,
    SENSOR_POSITION_THIGH,
    SENSOR_POSITION_BELT
};

enum processing_strategy {
    PROCESSING_RAW = 0,
    PROCESSING_PARTIAL,
    PROCESSING_FULL
};

enum step_algorithm_id {
    STEP_ALG_NONE = 0,
    STEP_ALG_PEAK,
    STEP_ALG_ADAPTIVE_PEAK,
    STEP_ALG_ZERO_CROSSING,
    STEP_ALG_FFT,
    STEP_ALG_AUTOCORR
};

struct accel_record {
    uint32_t sample_id;
    uint32_t t_ms;
    int16_t ax;
    int16_t ay;
    int16_t az;
};

struct step_count_result {
    uint32_t steps;
    uint32_t candidate_events;
    uint32_t rejected_events;
    int32_t confidence_permille;
    int error_code;
};

#endif
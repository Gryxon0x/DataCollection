cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(DataCollection)

target_include_directories(app PRIVATE
    include
    drivers/bma400
)

target_sources(app PRIVATE
    src/main.c

    src/app/app_commands.c
    src/app/app_state.c
    src/app/app_session.c

    src/sensor/bma400_app.c

    src/transport/ble_data_service.c
    src/transport/ble_protocol.c

    src/step/step_registry.c
    src/step/step_preprocess.c
    src/step/step_profiles.c

    src/step/algorithms/step_alg_none.c
    src/step/algorithms/step_alg_peak.c
    src/step/algorithms/step_alg_adaptive_peak.c
    src/step/algorithms/step_alg_zero_crossing.c
    src/step/algorithms/step_alg_fft.c
    src/step/algorithms/step_alg_autocorr.c

    drivers/bma400/bma400.c
)

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(DataCollection)

target_include_directories(app PRIVATE
    include
    drivers/bma400
)

target_sources(app PRIVATE
    src/main.c
    src/bma400_app.c
    drivers/bma400/bma400.c
)

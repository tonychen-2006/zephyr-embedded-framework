cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(project)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE 
    src/main.c
    src/bus/app_bus.c
    src/modules/comms/comms_uart.c
    src/modules/sensor/sensor_module.c
    src/controller.c
    src/actuator.c
    src/modules/comms/comms_ble.c
)
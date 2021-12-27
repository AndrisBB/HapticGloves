cmake_minimum_required(VERSION 3.20.0)

set(BOARD nrf52840dongle_nrf52840)

find_package(Zephyr)
project(nrf52_haptic)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/bt_service.c)
target_sources(app PRIVATE src/system_state.c)

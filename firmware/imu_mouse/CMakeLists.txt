cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(imu_mouse)

FILE(GLOB app_sources src/*.c)
list(FILTER app_sources EXCLUDE REGEX "/(hids_mouse|ble_gatt)\\.c$")
target_sources(app PRIVATE ${app_sources})

if(CONFIG_IM_BLE_DEVICE_MOUSE)
  target_sources(app PRIVATE src/hids_mouse.c)
endif()
if(CONFIG_IM_BLE_DEVICE_GATT)
  target_sources(app PRIVATE src/ble_gatt.c)
endif()

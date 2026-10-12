cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(imu_mouse)

FILE(GLOB app_sources src/*.c)
list(FILTER app_sources EXCLUDE REGEX "/(hids_mouse|imu_ble)\\.c$")
target_sources(app PRIVATE ${app_sources})

if(CONFIG_IM_BLE_DEVICE_MOUSE)
  target_sources(app PRIVATE src/hids_mouse.c)
endif()
if(CONFIG_IM_BLE_DEVICE_IMU)
  target_sources(app PRIVATE src/imu_ble.c)
endif()

cmake_minimum_required(VERSION 3.13.1)
# set(BOARD esp32s3)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(esp32s3_demo_edgeimpulse LANGUAGES C)
target_sources(app PRIVATE
    src/main.c
    src/wifi.c
)

# Minimum CMake version
cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF project configuration
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project name
project(serial_led_control)
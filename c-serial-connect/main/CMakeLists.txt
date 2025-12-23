# Minimum CMake version required
cmake_minimum_required(VERSION 3.16)

# Register this component with ESP-IDF
idf_component_register(
    SRCS "serial_led.c"          # Source files
    INCLUDE_DIRS "."             # Include directories
    REQUIRES                     # Required components
        driver
        freertos
        esp_timer
)
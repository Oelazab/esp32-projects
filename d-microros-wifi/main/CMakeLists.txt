# Minimum CMake version required
cmake_minimum_required(VERSION 3.16)

# Register this component with ESP-IDF
idf_component_register(
    SRCS "microros_led.c"        # Source files
    INCLUDE_DIRS "."             # Include directories
    REQUIRES                     # Required components
        driver
        freertos
        esp_timer
        nvs_flash
        esp_wifi
        esp_netif
        micro_ros_espidf_component
)

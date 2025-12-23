idf_component_register(
    SRCS "ping_example.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi lwip nvs_flash
)
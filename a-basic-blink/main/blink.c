// Include standard input/output library for printf() function
#include <stdio.h>

// Include FreeRTOS headers for real-time operating system functions
#include "freertos/FreeRTOS.h"

// Include FreeRTOS task management for delays and task scheduling
#include "freertos/task.h"

// Include GPIO driver for controlling General Purpose Input/Output pins
#include "driver/gpio.h"

// Include ESP32 logging utilities (not used in this code but included for completeness)
#include "esp_log.h"

// Define a constant for the GPIO pin number connected to the LED
// GPIO 2 is commonly connected to the onboard LED on most ESP32 development boards
#define BLINK_GPIO 2

// Main application function - this is the entry point for ESP32 programs
// Unlike standard C programs with main(), ESP32 uses app_main() as the starting point
void app_main(void)
{
    // Reset the GPIO pin to its default state before using it
    // This ensures the pin starts in a known, clean state
    gpio_reset_pin(BLINK_GPIO);

    // Set the GPIO pin direction to OUTPUT mode
    // This configures the pin to send signals out (drive the LED) rather than read input
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    // Print a startup message to the serial console
    // This appears in the serial monitor when you connect to the ESP32
    printf("ESP32 Blink Started!\n");

    // Infinite loop - ESP32 programs typically run forever
    // This keeps the program running continuously until power is removed
    while (1)
    {
        // Set the GPIO pin to HIGH voltage level (usually 3.3V)
        // Digital 1 = HIGH = 3.3V = LED turns ON (if active-high configuration)
        gpio_set_level(BLINK_GPIO, 1);

        // Print status message to serial console for debugging
        printf("LED ON\n");

        // Delay for 1000 milliseconds (1 second)
        // vTaskDelay() is a FreeRTOS function that suspends the task for the specified time
        // portTICK_PERIOD_MS converts milliseconds to FreeRTOS tick units
        // This allows other tasks to run during the delay (though we have only one task here)
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        // Set the GPIO pin to LOW voltage level (0V)
        // Digital 0 = LOW = 0V = LED turns OFF
        gpio_set_level(BLINK_GPIO, 0);

        // Print status message to serial console
        printf("LED OFF\n");

        // Delay for another 1000 milliseconds (1 second)
        // This creates the OFF part of the blink cycle
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        // The loop repeats forever: ON (1 sec) → OFF (1 sec) → ON (1 sec) → ...
    }

    // Note: This point is never reached because of the infinite while(1) loop
    // In ESP32 programming, the app_main() function should not return
    // If it did return, the ESP32 would restart
}
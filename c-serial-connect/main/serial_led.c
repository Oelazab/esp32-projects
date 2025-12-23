// Include standard input/output library for printf() and scanf() functions
#include <stdio.h>

// Include ESP32 FreeRTOS headers for task management and delays
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Include ESP32 GPIO driver for LED control
#include "driver/gpio.h"

// Include ESP32 UART driver for serial communication
#include "driver/uart.h"

// Include ESP32 logging utilities for debug messages
#include "esp_log.h"

// Include standard string library for string manipulation
#include <string.h>

// Define constants for LED GPIO pin
// GPIO 2 is usually the onboard LED on ESP32 development boards
#define LED_GPIO 2

// Define constants for UART (serial) configuration
#define UART_PORT_NUM UART_NUM_0 // Use UART0 (connected to USB)
#define UART_BAUD_RATE 115200    // Standard baud rate
#define UART_TXD_PIN 1           // GPIO 1 = TX pin
#define UART_RXD_PIN 3           // GPIO 3 = RX pin
#define UART_BUF_SIZE 1024       // Buffer size for incoming data

// Define command strings for LED control
#define CMD_ON "ON"         // Command to turn LED ON
#define CMD_OFF "OFF"       // Command to turn LED OFF
#define CMD_TOGGLE "TOGGLE" // Command to toggle LED state
#define CMD_BLINK "BLINK"   // Command to blink LED
#define CMD_HELP "HELP"     // Command to show help
#define CMD_EXIT "EXIT"     // Command to exit program

// Define log tag for ESP32 logging system
static const char *TAG = "SERIAL_LED";

// Global variable to track LED state (0=OFF, 1=ON)
static int led_state = 0;

// Function to initialize UART (serial communication)
void uart_init(void)
{
    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,           // Set baud rate to 115200
        .data_bits = UART_DATA_8_BITS,         // 8 data bits per byte
        .parity = UART_PARITY_DISABLE,         // No parity bit
        .stop_bits = UART_STOP_BITS_1,         // 1 stop bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // No hardware flow control
        .source_clk = UART_SCLK_APB,           // Use APB clock source
    };

    // Install UART driver with buffer
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM,
                                        UART_BUF_SIZE * 2, // Rx buffer size
                                        0,                 // Tx buffer size (0=no Tx buffer)
                                        0,                 // Queue size
                                        NULL,              // No queue handle
                                        0));               // No interrupt flags

    // Set UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));

    // Set UART pins (TX=GPIO1, RX=GPIO3 for UART0)
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM,
                                 UART_TXD_PIN,         // TX pin
                                 UART_RXD_PIN,         // RX pin
                                 UART_PIN_NO_CHANGE,   // No RTS pin
                                 UART_PIN_NO_CHANGE)); // No CTS pin

    // Log UART initialization
    ESP_LOGI(TAG, "UART initialized at %d baud", UART_BAUD_RATE);
}

// Function to initialize LED GPIO
void led_init(void)
{
    // Reset LED GPIO pin to default state
    gpio_reset_pin(LED_GPIO);

    // Set LED GPIO direction to OUTPUT (to control LED)
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    // Set initial LED state to OFF (LOW = 0V)
    gpio_set_level(LED_GPIO, 0);

    // Log LED initialization
    ESP_LOGI(TAG, "LED initialized on GPIO %d", LED_GPIO);
}

// Function to turn LED ON
void led_on(void)
{
    // Set GPIO pin to HIGH (3.3V) to turn LED ON
    gpio_set_level(LED_GPIO, 1);
    led_state = 1;             // Update global LED state
    printf("LED turned ON\n"); // Print status to serial
    ESP_LOGI(TAG, "LED turned ON");
}

// Function to turn LED OFF
void led_off(void)
{
    // Set GPIO pin to LOW (0V) to turn LED OFF
    gpio_set_level(LED_GPIO, 0);
    led_state = 0;              // Update global LED state
    printf("LED turned OFF\n"); // Print status to serial
    ESP_LOGI(TAG, "LED turned OFF");
}

// Function to toggle LED state
void led_toggle(void)
{
    // If LED is currently ON, turn it OFF; if OFF, turn it ON
    if (led_state == 1)
    {
        led_off(); // Turn LED OFF
    }
    else
    {
        led_on(); // Turn LED ON
    }
}

// Function to blink LED specified number of times
void led_blink(int times, int delay_ms)
{
    printf("Blinking LED %d times...\n", times);

    // Loop to blink LED specified number of times
    for (int i = 0; i < times; i++)
    {
        led_on();                                  // Turn LED ON
        vTaskDelay(delay_ms / portTICK_PERIOD_MS); // Wait specified delay

        led_off();                                 // Turn LED OFF
        vTaskDelay(delay_ms / portTICK_PERIOD_MS); // Wait specified delay
    }

    printf("Blink complete!\n");
    ESP_LOGI(TAG, "LED blinked %d times", times);
}

// Function to display help information
void show_help(void)
{
    printf("\n=== ESP32 Serial LED Control ===\n");
    printf("Available Commands:\n");
    printf("  ON      - Turn LED ON\n");
    printf("  OFF     - Turn LED OFF\n");
    printf("  TOGGLE  - Toggle LED state\n");
    printf("  BLINK   - Blink LED 5 times\n");
    printf("  BLINK N - Blink LED N times (e.g., BLINK 3)\n");
    printf("  STATUS  - Show current LED status\n");
    printf("  HELP    - Show this help message\n");
    printf("  EXIT    - Exit program (actually just stops accepting commands)\n");
    printf("\nType command and press Enter:\n");
}

// Function to show current LED status
void show_status(void)
{
    printf("LED Status: %s\n", led_state ? "ON" : "OFF");
    printf("LED GPIO: %d\n", LED_GPIO);
}

// Function to read a line from UART (serial)
int read_line(char *buffer, int max_len)
{
    int length = 0; // Track length of received string
    char ch;        // Temporary character buffer

    // Read characters until newline or buffer full
    while (length < max_len - 1)
    {
        // Read one character from UART
        int len = uart_read_bytes(UART_PORT_NUM, (uint8_t *)&ch, 1, 20 / portTICK_PERIOD_MS);

        // If character received
        if (len > 0)
        {
            // Check for carriage return or newline (end of command)
            if (ch == '\r' || ch == '\n')
            {
                buffer[length] = '\0'; // Null-terminate string
                return length;         // Return string length
            }
            // Handle backspace (delete character)
            else if (ch == '\b' || ch == 127)
            {
                if (length > 0)
                {
                    length--;        // Remove last character
                    printf("\b \b"); // Erase from terminal
                }
            }
            // Regular character
            else
            {
                buffer[length] = ch; // Add to buffer
                length++;            // Increment length
                printf("%c", ch);    // Echo character to terminal
            }
        }
    }

    buffer[length] = '\0'; // Null-terminate string
    return length;         // Return string length
}

// Function to process received command
void process_command(char *cmd)
{
    // Convert command to uppercase for case-insensitive comparison
    for (int i = 0; cmd[i]; i++)
    {
        if (cmd[i] >= 'a' && cmd[i] <= 'z')
        {
            cmd[i] = cmd[i] - 'a' + 'A';
        }
    }

    ESP_LOGI(TAG, "Processing command: %s", cmd);

    // Parse and execute command
    if (strcmp(cmd, CMD_ON) == 0)
    {
        led_on(); // Turn LED ON
    }
    else if (strcmp(cmd, CMD_OFF) == 0)
    {
        led_off(); // Turn LED OFF
    }
    else if (strcmp(cmd, CMD_TOGGLE) == 0)
    {
        led_toggle(); // Toggle LED state
    }
    else if (strncmp(cmd, CMD_BLINK, 5) == 0)
    {
        // Check if command has number argument (e.g., "BLINK 3")
        int blink_times = 5; // Default: blink 5 times
        if (strlen(cmd) > 5)
        {
            // Try to extract number from command
            sscanf(cmd + 5, "%d", &blink_times);

            // Validate number
            if (blink_times < 1 || blink_times > 20)
            {
                printf("Invalid number. Use 1-20.\n");
                blink_times = 5; // Use default
            }
        }
        led_blink(blink_times, 200); // Blink with 200ms delay
    }
    else if (strcmp(cmd, "STATUS") == 0)
    {
        show_status(); // Show LED status
    }
    else if (strcmp(cmd, CMD_HELP) == 0)
    {
        show_help(); // Show help message
    }
    else if (strcmp(cmd, CMD_EXIT) == 0)
    {
        printf("Exiting command mode. Press reset to restart.\n");
        ESP_LOGI(TAG, "Exit command received");
        // Note: We can't actually exit, but we can stop processing
    }
    else if (strlen(cmd) > 0)
    {
        printf("Unknown command: %s\n", cmd);
        printf("Type HELP for available commands.\n");
    }
}

// Main application function - entry point for ESP32 program
void app_main(void)
{
    char command_buffer[64]; // Buffer to store received commands

    // Set log level to INFO for debugging
    esp_log_level_set("*", ESP_LOG_INFO);

    // Initialize LED
    led_init();

    // Initialize UART (serial communication)
    uart_init();

    // Show startup message
    printf("\n\n");
    printf("========================================\n");
    printf("   ESP32 Serial LED Control Program\n");
    printf("========================================\n");
    printf("Board: ESP32\n");
    printf("LED GPIO: %d\n", LED_GPIO);
    printf("Baud Rate: %d\n", UART_BAUD_RATE);
    printf("Compiled: %s %s\n", __DATE__, __TIME__);
    printf("========================================\n\n");

    // Show initial help
    show_help();

    // Main program loop
    while (1)
    {
        printf("\n> "); // Show command prompt

        // Read command from serial
        int len = read_line(command_buffer, sizeof(command_buffer));

        // Process command if something was received
        if (len > 0)
        {
            process_command(command_buffer);
        }

        // Small delay to prevent CPU hogging
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
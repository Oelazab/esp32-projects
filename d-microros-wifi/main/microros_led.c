// Include standard input/output library
#include <stdio.h>
#include <string.h>

// Include ESP32 FreeRTOS headers
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

// Include ESP32 drivers
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

// Include ESP32 WiFi components
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

// Include micro-ROS headers
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>

// Include ROS 2 message types
#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/string.h>
#include <std_msgs/msg/int32.h>

// WiFi Configuration - CHANGE THESE TO YOUR NETWORK
#define WIFI_SSID "ssid"
#define WIFI_PASS "pass"

// micro-ROS Agent Configuration
#define AGENT_IP "192.168.1.2" // IP address of your ROS 2 machine running micro-ROS agent
#define AGENT_PORT 8888

// LED GPIO Configuration
#define LED_GPIO 2 // Onboard LED on most ESP32 boards

// WiFi connection bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

// Log tag
static const char *TAG = "MICROROS_LED";

// Global variables
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static int led_state = 0;

// micro-ROS variables
rcl_subscription_t led_control_subscriber;
rcl_subscription_t led_command_subscriber;
rcl_publisher_t led_status_publisher;
std_msgs__msg__Bool led_control_msg;
std_msgs__msg__String led_command_msg;
std_msgs__msg__Bool led_status_msg;
std_msgs__msg__Int32 led_blink_subscriber;
rcl_subscription_t blink_subscriber;

// Function prototypes
void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);
void led_blink(int times, int delay_ms);

// ============================================================================
// LED Control Functions
// ============================================================================

void led_init(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
    led_state = 0;
    ESP_LOGI(TAG, "LED initialized on GPIO %d", LED_GPIO);
}

void led_on(void)
{
    gpio_set_level(LED_GPIO, 1);
    led_state = 1;
    ESP_LOGI(TAG, "LED turned ON");
}

void led_off(void)
{
    gpio_set_level(LED_GPIO, 0);
    led_state = 0;
    ESP_LOGI(TAG, "LED turned OFF");
}

void led_toggle(void)
{
    led_state = !led_state;
    gpio_set_level(LED_GPIO, led_state);
    ESP_LOGI(TAG, "LED toggled to %s", led_state ? "ON" : "OFF");
}

void led_blink(int times, int delay_ms)
{
    ESP_LOGI(TAG, "Blinking LED %d times", times);
    for (int i = 0; i < times; i++)
    {
        led_on();
        vTaskDelay(delay_ms / portTICK_PERIOD_MS);
        led_off();
        vTaskDelay(delay_ms / portTICK_PERIOD_MS);
    }
}

// ============================================================================
// WiFi Event Handler
// ============================================================================

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < 10)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "Connect to the AP failed");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// ============================================================================
// WiFi Initialization
// ============================================================================

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", WIFI_SSID);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

// ============================================================================
// micro-ROS Callback Functions
// ============================================================================

// Callback for /led_control topic (std_msgs/Bool)
void led_control_callback(const void *msgin)
{
    const std_msgs__msg__Bool *msg = (const std_msgs__msg__Bool *)msgin;

    if (msg->data)
    {
        led_on();
    }
    else
    {
        led_off();
    }

    // Publish status update
    led_status_msg.data = led_state;
    rcl_publish(&led_status_publisher, &led_status_msg, NULL);
}

// Callback for /led_command topic (std_msgs/String)
void led_command_callback(const void *msgin)
{
    const std_msgs__msg__String *msg = (const std_msgs__msg__String *)msgin;

    ESP_LOGI(TAG, "Received command: %s", msg->data.data);

    // Convert to uppercase for comparison
    char cmd[64];
    strncpy(cmd, msg->data.data, sizeof(cmd) - 1);
    for (int i = 0; cmd[i]; i++)
    {
        if (cmd[i] >= 'a' && cmd[i] <= 'z')
        {
            cmd[i] = cmd[i] - 'a' + 'A';
        }
    }

    if (strcmp(cmd, "ON") == 0)
    {
        led_on();
    }
    else if (strcmp(cmd, "OFF") == 0)
    {
        led_off();
    }
    else if (strcmp(cmd, "TOGGLE") == 0)
    {
        led_toggle();
    }
    else if (strncmp(cmd, "BLINK", 5) == 0)
    {
        int times = 5;
        if (strlen(cmd) > 5)
        {
            sscanf(cmd + 5, "%d", &times);
        }
        led_blink(times, 200);
    }
    else
    {
        ESP_LOGW(TAG, "Unknown command: %s", cmd);
    }

    // Publish status update
    led_status_msg.data = led_state;
    rcl_publish(&led_status_publisher, &led_status_msg, NULL);
}

// Callback for /led_blink topic (std_msgs/Int32)
void led_blink_callback(const void *msgin)
{
    const std_msgs__msg__Int32 *msg = (const std_msgs__msg__Int32 *)msgin;

    int times = msg->data;
    if (times < 1)
        times = 1;
    if (times > 20)
        times = 20;

    ESP_LOGI(TAG, "Blink command received: %d times", times);
    led_blink(times, 200);

    // Publish status update
    led_status_msg.data = led_state;
    rcl_publish(&led_status_publisher, &led_status_msg, NULL);
}

// ============================================================================
// micro-ROS Task
// ============================================================================

void microros_task(void *arg)
{
    // Configure micro-ROS transport to WiFi
    rmw_uros_set_custom_transport(
        true,
        (void *)AGENT_IP,
        (void *)AGENT_PORT,
        (void *)4,
        (void *)2);

    // Initialize allocator
    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;

    // Wait for agent to be available
    ESP_LOGI(TAG, "Waiting for micro-ROS agent...");
    while (rmw_uros_ping_agent(1000, 10) != RCL_RET_OK)
    {
        ESP_LOGI(TAG, "Waiting for agent...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "Connected to micro-ROS agent!");

    // Create init options
    rclc_support_init(&support, 0, NULL, &allocator);

    // Create node
    rcl_node_t node;
    rclc_node_init_default(&node, "esp32_led_controller", "", &support);
    ESP_LOGI(TAG, "micro-ROS node created");

    // Create subscribers
    rclc_subscription_init_default(
        &led_control_subscriber,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
        "/led_control");

    rclc_subscription_init_default(
        &led_command_subscriber,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
        "/led_command");

    rclc_subscription_init_default(
        &blink_subscriber,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        "/led_blink");

    // Create publisher
    rclc_publisher_init_default(
        &led_status_publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
        "/led_status");

    ESP_LOGI(TAG, "Publishers and subscribers created");

    // Initialize message memory
    led_command_msg.data.data = (char *)malloc(64 * sizeof(char));
    led_command_msg.data.size = 64;
    led_command_msg.data.capacity = 64;

    // Create executor
    rclc_executor_t executor;
    rclc_executor_init(&executor, &support.context, 3, &allocator);
    rclc_executor_add_subscription(&executor, &led_control_subscriber, &led_control_msg,
                                   &led_control_callback, ON_NEW_DATA);
    rclc_executor_add_subscription(&executor, &led_command_subscriber, &led_command_msg,
                                   &led_command_callback, ON_NEW_DATA);
    rclc_executor_add_subscription(&executor, &blink_subscriber, &led_blink_subscriber,
                                   &led_blink_callback, ON_NEW_DATA);

    ESP_LOGI(TAG, "Executor initialized. Ready to receive commands!");

    // Publish initial status
    led_status_msg.data = led_state;
    rcl_publish(&led_status_publisher, &led_status_msg, NULL);

    // Spin executor
    while (1)
    {
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    // Cleanup (will never reach here)
    rcl_subscription_fini(&led_control_subscriber, &node);
    rcl_subscription_fini(&led_command_subscriber, &node);
    rcl_subscription_fini(&blink_subscriber, &node);
    rcl_publisher_fini(&led_status_publisher, &node);
    rcl_node_fini(&node);
    rclc_support_fini(&support);
    free(led_command_msg.data.data);

    vTaskDelete(NULL);
}

// ============================================================================
// Main Application Entry Point
// ============================================================================

void app_main(void)
{
    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Print startup banner
    printf("\n\n");
    printf("========================================\n");
    printf("   ESP32 micro-ROS LED Control\n");
    printf("========================================\n");
    printf("Board: ESP32\n");
    printf("LED GPIO: %d\n", LED_GPIO);
    printf("WiFi SSID: %s\n", WIFI_SSID);
    printf("Agent IP: %s:%d\n", AGENT_IP, AGENT_PORT);
    printf("Compiled: %s %s\n", __DATE__, __TIME__);
    printf("========================================\n\n");

    // Initialize LED
    led_init();

    // Initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    wifi_init_sta();

    // Create micro-ROS task
    ESP_LOGI(TAG, "Starting micro-ROS task...");
    xTaskCreate(microros_task,
                "microros_task",
                8192,
                NULL,
                5,
                NULL);

    ESP_LOGI(TAG, "System initialized successfully!");
}

// Include standard input/output library for printf() function
#include <stdio.h>

// Include ESP32 networking headers for WiFi functionality
#include "esp_wifi.h"

// Include ESP32 networking headers for TCP/IP stack
#include "esp_netif.h"

// Include ESP32 event handling system
#include "esp_event.h"

// Include FreeRTOS headers for real-time operating system functions
#include "freertos/FreeRTOS.h"

// Include FreeRTOS task management for delays and task scheduling
#include "freertos/task.h"

// Include ESP32 logging utilities for debug messages
#include "esp_log.h"

// Include standard string library for string manipulation functions
#include "string.h"

// Include standard library for malloc/free
#include "stdlib.h"

// Include LwIP headers for Lightweight IP stack (network protocols)
#include "lwip/err.h"

// Include LwIP sockets API for network communication
#include "lwip/sockets.h"

// Include LwIP DNS resolver for hostname to IP address conversion
#include "lwip/netdb.h"

// Include ICMP protocol headers for ping functionality
#include "lwip/icmp.h"

// Include IP protocol headers for IP packet handling
#include "lwip/inet_chksum.h"

// Include IP address handling utilities
#include "lwip/inet.h"

// Include non-volatile storage for WiFi configuration
#include "nvs_flash.h"

// Define a tag for logging - appears in serial monitor output
static const char *TAG = "PING_EXAMPLE";

// Define WiFi credentials - REPLACE THESE WITH YOUR NETWORK INFO
#define WIFI_SSID "YOUR_WIFI_SSID"         // Your WiFi network name
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD" // Your WiFi password

// Define ping target - website to ping
#define PING_TARGET "8.8.8.8" // Google DNS - always responds to ping

// Define ping interval in milliseconds
#define PING_INTERVAL 2000 // Ping every 2 seconds

// Define ping packet size in bytes
#define PING_DATA_SIZE 32 // 32 bytes of data in ping packet

// Define ping timeout in milliseconds
#define PING_TIMEOUT 1000 // Wait 1 second for ping response

// Global variable to store target IP address
static ip_addr_t target_addr;

// Global counter for ping sequence numbers
static uint16_t ping_seq = 0;

// Function to handle WiFi events (connection, disconnection, etc.)
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    // Check if this is a WiFi station (client) event
    if (event_base == WIFI_EVENT)
    {
        // Handle specific WiFi events
        if (event_id == WIFI_EVENT_STA_START)
        {
            // Event: WiFi station started successfully
            ESP_LOGI(TAG, "WiFi station started");

            // Connect to the configured WiFi network
            esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            // Event: WiFi disconnected
            ESP_LOGW(TAG, "WiFi disconnected, trying to reconnect...");

            // Try to reconnect after 5 second delay
            vTaskDelay(5000 / portTICK_PERIOD_MS);

            // Attempt to reconnect to WiFi
            esp_wifi_connect();
        }
    }
    // Check if this is an IP event
    else if (event_base == IP_EVENT)
    {
        // Handle specific IP events
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            // Event: Got IP address from DHCP server
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

            // Log the obtained IP address
            ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));

            // Convert string IP to binary format and store in target_addr
            // ipaddr_addr() converts dotted-decimal string to 32-bit IP address
            ip4_addr_set_u32(&target_addr.u_addr.ip4, ipaddr_addr(PING_TARGET));

            // Log the target IP
            ESP_LOGI(TAG, "Ping target: %s", PING_TARGET);
        }
    }
}

// Function to initialize WiFi connection
static void wifi_init_sta(void)
{
    // Initialize the underlying TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop for handling system events
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi station (client) network interface
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Create event handler for WiFi and IP events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Configure WiFi as station (client) mode
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,                        // Set WiFi SSID from macro
            .password = WIFI_PASSWORD,                // Set WiFi password from macro
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // Minimum auth mode required
        },
    };

    // Set WiFi mode to station (client) only (not access point)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Apply the WiFi configuration
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Log that WiFi initialization is complete
    ESP_LOGI(TAG, "WiFi initialization finished");
}

// Function to send a ping (ICMP echo request) and wait for response
static void ping_target(void)
{
    // Create a raw socket for ICMP protocol
    // AF_INET = IPv4, SOCK_RAW = raw socket, IPPROTO_ICMP = ICMP protocol
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    // Check if socket creation failed
    if (sock < 0)
    {
        // Socket creation failed
        ESP_LOGE(TAG, "Failed to create socket: error %d", errno);
        return;
    }

    // Set socket timeout for receive operations
    struct timeval timeout;
    timeout.tv_sec = PING_TIMEOUT / 1000;           // Seconds part of timeout
    timeout.tv_usec = (PING_TIMEOUT % 1000) * 1000; // Microseconds part of timeout

    // Apply timeout to socket
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        ESP_LOGE(TAG, "Failed to set socket timeout");
        close(sock);
        return;
    }

    // Create ICMP packet (ping) structure
    struct icmp_echo_hdr *icmp_pkt;

    // Allocate memory for ICMP packet + data
    icmp_pkt = (struct icmp_echo_hdr *)malloc(sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE);

    // Check if memory allocation failed
    if (icmp_pkt == NULL)
    {
        // Memory allocation failed
        ESP_LOGE(TAG, "Failed to allocate memory for ICMP packet");
        close(sock);
        return;
    }

    // Initialize ICMP packet
    ICMPH_TYPE_SET(icmp_pkt, ICMP_ECHO); // Set type to Echo Request (8)
    ICMPH_CODE_SET(icmp_pkt, 0);         // Set code to 0
    icmp_pkt->chksum = 0;                // Initialize checksum to 0 (calculated later)
    icmp_pkt->id = 0xABCD;               // Set ICMP identifier (arbitrary value)
    icmp_pkt->seqno = htons(ping_seq);   // Set sequence number (converted to network byte order)
    ping_seq++;                          // Increment sequence for next ping

    // Fill packet data with incrementing numbers (0, 1, 2, 3...)
    char *data_ptr = (char *)icmp_pkt + sizeof(struct icmp_echo_hdr);
    for (int i = 0; i < PING_DATA_SIZE; i++)
    {
        data_ptr[i] = (char)i; // Fill with pattern 0, 1, 2, 3...
    }

    // Calculate ICMP checksum (error detection for packet)
    icmp_pkt->chksum = inet_chksum(icmp_pkt, sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE);

    // Prepare destination address structure
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr)); // Clear structure to all zeros

    // Set address family to IPv4
    dest_addr.sin_family = AF_INET;

    // Set destination IP address (from our global target_addr)
    dest_addr.sin_addr.s_addr = target_addr.u_addr.ip4.addr;

    // Send the ICMP packet (ping request)
    int sent = sendto(sock, icmp_pkt, sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE,
                      0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    // Check if send was successful
    if (sent < 0)
    {
        // Send failed
        ESP_LOGE(TAG, "Failed to send ping: error %d", errno);
    }
    else
    {
        // Send successful, now wait for response
        ESP_LOGI(TAG, "Ping #%d sent to %s", ping_seq, PING_TARGET);

        // Buffer for receiving response
        char recv_buf[256];
        struct sockaddr_in src_addr;
        socklen_t addr_len = sizeof(src_addr);

        // Get current tick count for timing (simpler alternative to esp_timer_get_time)
        TickType_t start_ticks = xTaskGetTickCount();

        // Receive response (blocking call with timeout)
        int received = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                                (struct sockaddr *)&src_addr, &addr_len);

        // Get tick count after receive attempt
        TickType_t end_ticks = xTaskGetTickCount();

        // Check if receive was successful
        if (received < 0)
        {
            // Receive failed (timeout or error)
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Timeout occurred
                ESP_LOGW(TAG, "No response from %s (timeout)", PING_TARGET);
            }
            else
            {
                // Other error occurred
                ESP_LOGE(TAG, "Receive error: %d", errno);
            }
        }
        else
        {
            // Receive successful - ping reply received

            // Calculate Round Trip Time in milliseconds
            // portTICK_PERIOD_MS = milliseconds per tick (usually 1ms)
            TickType_t rtt_ticks = end_ticks - start_ticks;
            float rtt_ms = (float)rtt_ticks * portTICK_PERIOD_MS;

            // Convert source IP to human-readable string
            char src_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &src_addr.sin_addr, src_ip, sizeof(src_ip));

            // Log successful ping with RTT
            ESP_LOGI(TAG, "Ping reply from %s: time=%.1f ms", src_ip, rtt_ms);
        }
    }

    // Free allocated memory for ICMP packet
    free(icmp_pkt);

    // Close the socket
    close(sock);
}

// Main application function - entry point for ESP32 program
void app_main(void)
{
    // Initialize non-volatile storage (used by WiFi)
    esp_err_t ret = nvs_flash_init();

    // Check if NVS initialization failed due to partition being erased
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Erase NVS partition and try again
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // Check final NVS initialization result
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi connection
    wifi_init_sta();

    // Wait a moment for WiFi connection to establish
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // Infinite loop for continuous pinging
    while (1)
    {
        // Check if we have a valid target IP address (not zero)
        if (target_addr.u_addr.ip4.addr != 0)
        {
            // We have an IP address, send ping
            ping_target();
        }
        else
        {
            // No IP address yet (WiFi not connected or IP not assigned)
            ESP_LOGW(TAG, "Waiting for WiFi connection and IP address...");

            // Check WiFi connection status
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
            {
                ESP_LOGI(TAG, "Connected to AP: %s, RSSI: %d",
                         ap_info.ssid, ap_info.rssi);
            }
        }

        // Wait before next ping (PING_INTERVAL milliseconds)
        vTaskDelay(PING_INTERVAL / portTICK_PERIOD_MS);
    }
}
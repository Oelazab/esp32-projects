# ESP32 Blink from Terminal

Simple ESP32 blink example without IDE, using only terminal commands.


## Quick Start

## 1. **Install ESP-IDF**

Open **PowerShell** or **Command Prompt**:

```powershell
# Create workspace directory
mkdir %USERPROFILE%\esp
cd %USERPROFILE%\esp

# Clone ESP-IDF
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git

# Install ESP-IDF tools
cd esp-idf
./install.ps1  # For PowerShell
# OR if using cmd: install.bat
```

## 4. **Create Blink Source Code**

## **Repository Structure**
```
a-basic-blink/
├── main/
│   ├── CMakeLists.txt
│   └── blink.c
├── CMakeLists.txt
└── README.md
```

## **File Contents**

### **1. `main/blink.c`**
```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BLINK_GPIO 2  // GPIO 2 is usually the onboard LED

void app_main(void)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    
    printf("ESP32 Blink Started!\n");
    
    while (1) {
        gpio_set_level(BLINK_GPIO, 1);
        printf("LED ON\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        
        gpio_set_level(BLINK_GPIO, 0);
        printf("LED OFF\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
```

### **2. `main/CMakeLists.txt`**
```cmake
idf_component_register(
    SRCS "blink.c"
    INCLUDE_DIRS "."
)
```

### **3. `CMakeLists.txt` (root)**
```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(blink)
```


## 5. **Configure and Build**

```powershell
# First, set up the environment in each terminal session
cd %USERPROFILE%\esp\esp-idf
.\export.ps1  # For PowerShell
# OR for cmd: export.bat

# Navigate to your project
cd %USERPROFILE%\esp32-projects\a-basic-blink

# Set target to ESP32 (if not already set)
idf.py set-target esp32

# Configure the project (creates sdkconfig)
idf.py menuconfig  # Optional - press Q to exit if default is fine

# Build the project
idf.py build
```

## 6. **Upload to ESP32**

### First, identify your COM port:
```powershell
# Check connected COM ports
mode  # or look in Device Manager under "Ports (COM & LPT)"
```

### Flash the firmware:
```powershell
# Replace COM3 with your actual port
idf.py -p COM3 flash

# Or flash and monitor at the same time
idf.py -p COM3 flash monitor

# To exit monitor: Ctrl + ]
```


## 7. **ESP32 Board**
![Alt](assets/test.gif "ESP32-Board")
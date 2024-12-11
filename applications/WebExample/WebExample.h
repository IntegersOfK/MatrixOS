/*
This is the example application for Matrix OS
Remember to include this header file in the Applications.h in your device family folder (devices/<Device Family>/Applications.h)

What this application does:
Any pressed will lit up the key (in a user defined color)
Click the function key will open an menu UI
Top left button will open a number selector UI that saves teh value into the number variable
Top right button will open a color picker UI that saves the value into the color variable. This also changes the color of the button pressed
Click the function key in the menu will exit the UI
Hold the function key in the menu will exit the application
Midi signal received will be echoed back to the host
*/

#pragma once

#include "MatrixOS.h"
#include "applications/Application.h"
#include "applications/BrightnessControl/BrightnessControl.h" 

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_http_client.h"

#define WEB_EXAMPLE_WIFI_SSID "AAA"
#define WEB_EXAMPLE_WIFI_PASS "15elephants"
#define WEB_EXAMPLE_MAXIMUM_RETRY  3
#define WEB_EXAMPLE_SCAN_LIST_SIZE 16

#define WEB_SERVER "192.168.1.18:8123"
#define WEB_PATH "/api/webhook/rgb_webhook"

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define TAG "Web Example"

class WebExample : public Application {
 public:
  static Application_Info info;
  bool is_connected = false;

  Color color = Color(0xFFFFFF);

  void Setup() override;
  void Loop() override;
  void End() override;

  bool SetUpWIFI();
  void UIMenu();

  static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
  void wifi_init_sta(void);
  void send_post_request(const char* post_data);
  void print_auth_mode(int authmode);
  void print_cipher_type(int pairwise_cipher, int group_cipher);
};

// Meta data about this application
inline Application_Info WebExample::info = {
    .name = "Web Example",
    .author = "203 Systems",
    .color = Color(0xFFFFFF),
    .version = 1,
    .visibility = true,
};

// Register this Application to the OS (Use the class name of your application as the variable)
REGISTER_APPLICATION(WebExample);
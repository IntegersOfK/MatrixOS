#include "WebExample.h"
#include "ui/UI.h"  // Include the UI Framework
#include "ui/UIUtilities.h"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static uint8_t s_retry_num = 0;
// THIS IS BAD! We are defining a global variable in app file. This is bad practice.
/// This means this mem is wasted regardless of the application is running or not.

// Run once
void WebExample::Setup() {
  MLOGI("Example", "Example Started");

  wifi_init_sta();  // I'm not sure if I can recall this or safely undo this in Exit();

  UIMenu();  // Enter the UI Menu directly
  Exit();    // Exit the application if the menu is exited
}

// Run in a loop after Setup()
void WebExample::Loop() {}

void WebExample::UIMenu() {
  // Matrix OS Debug Log, sent to hardware UART and USB CDC
  MLOGI("Example", "Enter UI Menu");

  // Create a UI Object
  // UI Name, Color (as the text scroll color). and new led layer (Set as true, the UI will render on a new led layer. Persevere what was rendered before after UI exits)
  UI menu("UI Menu", Color(0x00FFFF), true);

  // Create an dynamic colored button
  UIButton colorSelector;
  colorSelector.SetName("Color Selector");                       // Name of this UI element
  colorSelector.SetColorFunc([&]() -> Color { return color; });  // Use the color variable as the color of this UI element
  colorSelector.SetSize(Dimension(8, 1));                             // Size of the UI element (2x1)
  colorSelector.OnPress([&]() -> void {                          // Callback function when the button is pressed
    MatrixOS::UIUtility::ColorPicker(color);  // References to the color variable. The color variable will be updated by the ColorPicker function. Return true if color is
                                                // changed, false if not.
    char json_data[64];
    sprintf(json_data, "{\"color\":%lu}", color.RGB());
    send_post_request(json_data);
  });

  // Add the UI element to the UI object to top right conner
  menu.AddUIComponent(colorSelector, Point(0, Device::y_size - 1));

  UIButton wifiConnected;
  wifiConnected.SetName("Wifi status");
  wifiConnected.SetColorFunc([&]() -> Color { return is_connected ? Color(0x00FF00) : Color(0xFF0000); });
  wifiConnected.OnHold([&]() -> void {
    if (is_connected)
    {
      MatrixOS::UIUtility::TextScroll("Wifi is connected", Color(0x00FF00));
    }
    else
    {
      MatrixOS::UIUtility::TextScroll("Wifi is not connected", Color(0xFF0000));
    }
  });

    menu.AddUIComponent(wifiConnected, Point(0, 0));

  // Set a key event handler for the UI object
  // By default, the UI exits after the function key is PRESSED.
  // Since this is the main UI for this application.
  // We want to exit the application when the function key is hold down,
  // and exit the UI is released (but before the hold down time threshold)

  // First, disable the default exit behavior
  menu.AllowExit(false);

  // Second, set the key event handler to match the intended behavior
  menu.SetKeyEventHandler([&](KeyEvent* keyEvent) -> bool {
    // If function key is hold down. Exit the application
    if (keyEvent->id == FUNCTION_KEY)
    {
      if (keyEvent->info.state == HOLD)
      {
        Exit();  // Exit the application.

        return true;  // Block UI from to do anything with FN, basically this function control the life cycle of the UI. This is not really needed as the application exits after
                      // Exit();
      }
      else if (keyEvent->info.state == RELEASED)
      {
        menu.Exit();  // Exit the UI
        return true;  // Block UI from to do anything with FN, basically this function control the life cycle of the UI
      }
    }
    return false;  // Nothing happened. Let the UI handle the key event
  });

  // The UI object is now fully set up. Let the UI runtime to start and take over.
  menu.Start();
  // Once the UI is exited (Not the application exit!), the code will continue here.
  // If Exit() is called in UI. The code will start in the End() of this application and then exit.

  // See /os/framework/ui/UI.h for more UI Framework API
  // See /os/framework/ui/UIComponents.h for more UI Components
  // See /os/framework/ui/UIInterface.h for more UI built in UI Interface

  // You can also create your own UI Components and UI Interfaces for your own application.
  // You can see the Note application for an example of how to do that. (Note Pad. Octave Shifter. Scales, ScaleVisualizer...)

  MLOGI("Example", "Exited UI Menu");
}

void WebExample::End() {
  MLOGI("Example", "Example Exited");
}

void WebExample::wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    if (s_retry_num < WEB_EXAMPLE_MAXIMUM_RETRY)
    {
      esp_wifi_connect();
      s_retry_num++;
      MLOGI(TAG, "retry to connect to the AP");
    }
    else
    {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    MLOGI(TAG, "connect to the AP fail");
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    MLOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void WebExample::wifi_init_sta(void) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WebExample::wifi_event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WebExample::wifi_event_handler, NULL, &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta = {.ssid = WEB_EXAMPLE_WIFI_SSID, .password = WEB_EXAMPLE_WIFI_PASS, .scan_method = WIFI_ALL_CHANNEL_SCAN},
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  MLOGI(TAG, "wifi_init_sta finished.");

  uint16_t number = WEB_EXAMPLE_SCAN_LIST_SIZE;
  wifi_ap_record_t ap_info[WEB_EXAMPLE_SCAN_LIST_SIZE];
  uint16_t ap_count = 0;
  memset(ap_info, 0, sizeof(ap_info));

  wifi_scan_config_t scan_config = {.ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true};
  esp_wifi_scan_start(&scan_config, true);

  MLOGI(TAG, "Max AP number ap_info can hold = %u", number);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
  MLOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
  for (int i = 0; i < number; i++)
  {
    MLOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
    MLOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
    print_auth_mode(ap_info[i].authmode);
    if (ap_info[i].authmode != WIFI_AUTH_WEP)
    {
      print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
    }
    MLOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
  }

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT)
  {
    MLOGI(TAG, "connected to ap SSID:%s password:%s", WEB_EXAMPLE_WIFI_SSID, WEB_EXAMPLE_WIFI_PASS);
    is_connected = true;
  }
  else if (bits & WIFI_FAIL_BIT)
  {
    MLOGI(TAG, "Failed to connect to SSID:%s, password:%s", WEB_EXAMPLE_WIFI_SSID, WEB_EXAMPLE_WIFI_PASS);
  }
  else
  {
    MLOGE(TAG, "UNEXPECTED EVENT");
  }
}

void WebExample::send_post_request(const char* post_data) {
  esp_http_client_config_t config = {
      .url = "http://" WEB_SERVER WEB_PATH,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);

  // Set request method and payload
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_post_field(client, post_data, strlen(post_data));

  // Send the request
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
  {
    int status_code = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    MLOGI(TAG, "HTTP POST Status = %d, content_length = %d", status_code, content_length);
  }
  else
  {
    MLOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
  }

  // Clean up
  esp_http_client_cleanup(client);
}

void WebExample::print_auth_mode(int authmode) {
  switch (authmode)
  {
    case WIFI_AUTH_OPEN:
      MLOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
      break;
    case WIFI_AUTH_OWE:
      MLOGI(TAG, "Authmode \tWIFI_AUTH_OWE");
      break;
    case WIFI_AUTH_WEP:
      MLOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
      break;
    case WIFI_AUTH_WPA_PSK:
      MLOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
      break;
    case WIFI_AUTH_WPA2_PSK:
      MLOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
      break;
    case WIFI_AUTH_WPA_WPA2_PSK:
      MLOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
      break;
    case WIFI_AUTH_ENTERPRISE:
      MLOGI(TAG, "Authmode \tWIFI_AUTH_ENTERPRISE");
      break;
    case WIFI_AUTH_WPA3_PSK:
      MLOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
      break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
      MLOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
      break;
    case WIFI_AUTH_WPA3_ENT_192:
      MLOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENT_192");
      break;
    default:
      MLOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
      break;
  }
}

void WebExample::print_cipher_type(int pairwise_cipher, int group_cipher) {
  switch (pairwise_cipher)
  {
    case WIFI_CIPHER_TYPE_NONE:
      MLOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
      break;
    case WIFI_CIPHER_TYPE_WEP40:
      MLOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
      break;
    case WIFI_CIPHER_TYPE_WEP104:
      MLOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
      break;
    case WIFI_CIPHER_TYPE_TKIP:
      MLOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
      break;
    case WIFI_CIPHER_TYPE_CCMP:
      MLOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
      break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
      MLOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
      break;
    case WIFI_CIPHER_TYPE_AES_CMAC128:
      MLOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_AES_CMAC128");
      break;
    case WIFI_CIPHER_TYPE_SMS4:
      MLOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_SMS4");
      break;
    case WIFI_CIPHER_TYPE_GCMP:
      MLOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP");
      break;
    case WIFI_CIPHER_TYPE_GCMP256:
      MLOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP256");
      break;
    default:
      MLOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
      break;
  }

  switch (group_cipher)
  {
    case WIFI_CIPHER_TYPE_NONE:
      MLOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
      break;
    case WIFI_CIPHER_TYPE_WEP40:
      MLOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
      break;
    case WIFI_CIPHER_TYPE_WEP104:
      MLOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
      break;
    case WIFI_CIPHER_TYPE_TKIP:
      MLOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
      break;
    case WIFI_CIPHER_TYPE_CCMP:
      MLOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
      break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
      MLOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
      break;
    case WIFI_CIPHER_TYPE_SMS4:
      MLOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_SMS4");
      break;
    case WIFI_CIPHER_TYPE_GCMP:
      MLOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP");
      break;
    case WIFI_CIPHER_TYPE_GCMP256:
      MLOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP256");
      break;
    default:
      MLOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
      break;
  }
}
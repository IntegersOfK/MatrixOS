#include "HANet.h"
#include "HAConfig.h"

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <new>

namespace HANet
{
static const char* TAG = "HANet";

static constexpr uint8_t REQUEST_QUEUE_DEPTH = 8;
static constexpr uint8_t RESPONSE_QUEUE_DEPTH = 8;
static constexpr uint32_t WORKER_STACK_BYTES = 8192; // esp_http_client + lwIP need the headroom
static constexpr UBaseType_t WORKER_PRIORITY = 2;
static constexpr int HTTP_TIMEOUT_MS = 3000;
static constexpr uint32_t CONNECT_WAIT_MS = 3000; // How long a queued request waits for Wi-Fi before failing
static constexpr uint32_t RECONNECT_MIN_MS = 1000;
static constexpr uint32_t RECONNECT_MAX_MS = 30000;
static constexpr EventBits_t CONNECTED_BIT = BIT0;

struct PendingRequest {
  Method method;
  string path;
  string body;
  uint32_t tag;
};

static bool started = false;
static std::atomic<LinkState> linkState{LinkState::Unconfigured};
static std::atomic<uint8_t> lastDisconnectReason{0};
static uint32_t reconnectDelayMs = RECONNECT_MIN_MS; // Only touched from the default event loop task
static QueueHandle_t requestQueue = nullptr;
static QueueHandle_t responseQueue = nullptr;
static EventGroupHandle_t linkEvents = nullptr;
static TimerHandle_t reconnectTimer = nullptr;

// Only touched by the worker task (and the HTTP event callback, which runs inside perform on that task)
static esp_http_client_handle_t client = nullptr;
static Response* activeResponse = nullptr;

bool IsWifiConfigured() {
  return sizeof(HA_WIFI_SSID) > 1;
}

bool HasToken() {
  return sizeof(HA_TOKEN) > 1;
}

LinkState GetLinkState() {
  return linkState.load();
}

uint8_t GetLastDisconnectReason() {
  return lastDisconnectReason.load();
}

static bool Check(esp_err_t err, const char* what) {
  if (err != ESP_OK)
  {
    MLOGE(TAG, "%s failed: %s", what, esp_err_to_name(err));
    return false;
  }
  return true;
}

static void ReconnectTimerCallback(TimerHandle_t timer) {
  (void)timer;
  esp_wifi_connect();
}

// Runs on the shared sys_evt task, whose stack is only 2304 bytes: MLOG overflows it, so keep this to state updates.
// The test page shows the link state and the last disconnect reason instead.
static void WifiEventHandler(void* arg, esp_event_base_t base, int32_t id, void* data) {
  (void)arg;
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
  {
    wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)data;
    lastDisconnectReason = event->reason;
    linkState = LinkState::Connecting;
    xEventGroupClearBits(linkEvents, CONNECTED_BIT);
    // Backoff on a timer: blocking here would stall the shared default event loop
    xTimerChangePeriod(reconnectTimer, pdMS_TO_TICKS(reconnectDelayMs), 0);
    reconnectDelayMs = std::min<uint32_t>(reconnectDelayMs * 2, RECONNECT_MAX_MS);
  }
  else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
  {
    reconnectDelayMs = RECONNECT_MIN_MS;
    linkState = LinkState::Connected;
    xEventGroupSetBits(linkEvents, CONNECTED_BIT);
  }
}

static bool StartWifi() {
  // The OS may already own the netif layer or the default loop; both are fine to share
  esp_err_t err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
  {
    return Check(err, "esp_netif_init");
  }
  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
  {
    return Check(err, "esp_event_loop_create_default");
  }
  if (esp_netif_create_default_wifi_sta() == nullptr)
  {
    MLOGE(TAG, "esp_netif_create_default_wifi_sta failed");
    return false;
  }

  wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
  if (!Check(esp_wifi_init(&initConfig), "esp_wifi_init"))
  {
    return false;
  }
  if (!Check(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiEventHandler, nullptr, nullptr), "register WIFI_EVENT") ||
      !Check(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiEventHandler, nullptr, nullptr), "register IP_EVENT"))
  {
    return false;
  }

  wifi_config_t wifiConfig = {};
  strlcpy((char*)wifiConfig.sta.ssid, HA_WIFI_SSID, sizeof(wifiConfig.sta.ssid));
  strlcpy((char*)wifiConfig.sta.password, HA_WIFI_PASSWORD, sizeof(wifiConfig.sta.password));
  wifiConfig.sta.threshold.authmode = sizeof(HA_WIFI_PASSWORD) > 1 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

  // RAM storage: credentials come from HAConfig.local.h, so don't leave a second copy in the Wi-Fi NVS namespace
  return Check(esp_wifi_set_storage(WIFI_STORAGE_RAM), "esp_wifi_set_storage") &&
         Check(esp_wifi_set_mode(WIFI_MODE_STA), "esp_wifi_set_mode") &&
         Check(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig), "esp_wifi_set_config") && Check(esp_wifi_start(), "esp_wifi_start");
}

static esp_err_t HttpEventHandler(esp_http_client_event_t* event) {
  if (event->event_id == HTTP_EVENT_ON_DATA && activeResponse != nullptr && event->data_len > 0)
  {
    size_t room = MAX_RESPONSE_BODY - activeResponse->body.size();
    activeResponse->body.append((const char*)event->data, std::min<size_t>(room, (size_t)event->data_len));
  }
  return ESP_OK;
}

static bool EnsureClient() {
  if (client != nullptr)
  {
    return true;
  }

  esp_http_client_config_t config = {};
  config.host = HA_HOST;
  config.port = HA_PORT;
  config.path = "/api/";
  config.transport_type = HTTP_TRANSPORT_OVER_TCP;
  config.timeout_ms = HTTP_TIMEOUT_MS;
  config.disable_auto_redirect = true;
  config.event_handler = HttpEventHandler;
  client = esp_http_client_init(&config);
  if (client == nullptr)
  {
    MLOGE(TAG, "esp_http_client_init failed, free heap %lu", (unsigned long)esp_get_free_heap_size());
    return false;
  }
  // Headers persist on the handle across requests
  esp_http_client_set_header(client, "Authorization", "Bearer " HA_TOKEN);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  return true;
}

static void Perform(PendingRequest* request, Response* response) {
  response->tag = request->tag;

  EventBits_t bits = xEventGroupWaitBits(linkEvents, CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(CONNECT_WAIT_MS));
  if ((bits & CONNECTED_BIT) == 0)
  {
    response->error = ESP_ERR_WIFI_NOT_CONNECT;
    return;
  }
  if (!EnsureClient())
  {
    response->error = ESP_ERR_NO_MEM;
    return;
  }

  string url = "http://" HA_HOST ":" + std::to_string(HA_PORT) + request->path;
  esp_http_client_set_url(client, url.c_str());
  if (request->method == Method::Post)
  {
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, request->body.c_str(), request->body.size());
  }
  else
  {
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_post_field(client, nullptr, 0);
  }

  activeResponse = response;
  esp_err_t err = esp_http_client_perform(client);
  activeResponse = nullptr;

  response->error = err;
  if (err == ESP_OK)
  {
    response->status = esp_http_client_get_status_code(client);
    if (response->status >= 300)
    {
      MLOGW(TAG, "%s -> HTTP %d", request->path.c_str(), response->status);
    }
  }
  else
  {
    // Drop the handle so the next request starts from a clean connection
    MLOGW(TAG, "%s -> %s", request->path.c_str(), esp_err_to_name(err));
    esp_http_client_cleanup(client);
    client = nullptr;
  }
}

static void PushResponse(Response* response) {
  if (xQueueSend(responseQueue, &response, 0) == pdTRUE)
  {
    return;
  }
  // Nobody is polling (app closed or busy): drop the oldest so memory stays bounded
  Response* oldest = nullptr;
  if (xQueueReceive(responseQueue, &oldest, 0) == pdTRUE)
  {
    delete oldest;
  }
  if (xQueueSend(responseQueue, &response, 0) != pdTRUE)
  {
    delete response;
  }
}

static void WorkerTask(void* parameters) {
  (void)parameters;
  PendingRequest* request = nullptr;
  while (true)
  {
    if (xQueueReceive(requestQueue, &request, portMAX_DELAY) != pdTRUE)
    {
      continue;
    }
    Response* response = new (std::nothrow) Response();
    if (response != nullptr)
    {
      Perform(request, response);
    }
    delete request;
    if (response != nullptr)
    {
      PushResponse(response);
    }
  }
}

void Begin() {
  if (started)
  {
    return;
  }
  if (!IsWifiConfigured())
  {
    MLOGW(TAG, "No Wi-Fi SSID; copy HAConfig.example.h to HAConfig.local.h and rebuild");
    return;
  }
  // From here on never retry: re-running the Wi-Fi init sequence is what rebooted the 2024 prototype
  started = true;

  requestQueue = xQueueCreate(REQUEST_QUEUE_DEPTH, sizeof(PendingRequest*));
  responseQueue = xQueueCreate(RESPONSE_QUEUE_DEPTH, sizeof(Response*));
  linkEvents = xEventGroupCreate();
  reconnectTimer = xTimerCreate("haReconnect", pdMS_TO_TICKS(RECONNECT_MIN_MS), pdFALSE, nullptr, ReconnectTimerCallback);
  if (requestQueue == nullptr || responseQueue == nullptr || linkEvents == nullptr || reconnectTimer == nullptr)
  {
    MLOGE(TAG, "Out of memory creating queues, free heap %lu", (unsigned long)esp_get_free_heap_size());
    linkState = LinkState::Failed;
    return;
  }

  linkState = LinkState::Connecting;
  if (!StartWifi())
  {
    linkState = LinkState::Failed;
    return;
  }
  if (xTaskCreate(WorkerTask, "haNet", WORKER_STACK_BYTES, nullptr, WORKER_PRIORITY, nullptr) != pdPASS)
  {
    MLOGE(TAG, "Failed to create worker task");
    linkState = LinkState::Failed;
    return;
  }
  MLOGI(TAG, "Started, free heap %lu", (unsigned long)esp_get_free_heap_size());
}

bool Request(Method method, const string& path, const string& body, uint32_t tag) {
  if (!started || linkState == LinkState::Failed || requestQueue == nullptr)
  {
    return false;
  }
  PendingRequest* request = new (std::nothrow) PendingRequest{method, path, body, tag};
  if (request == nullptr)
  {
    return false;
  }
  if (xQueueSend(requestQueue, &request, 0) != pdTRUE)
  {
    MLOGW(TAG, "Request queue full, dropped %s", path.c_str());
    delete request;
    return false;
  }
  return true;
}

bool CallService(const string& domain, const string& service, const string& jsonBody, uint32_t tag) {
  return Request(Method::Post, "/api/services/" + domain + "/" + service, jsonBody, tag);
}

bool PollResponse(Response* response) {
  if (responseQueue == nullptr)
  {
    return false;
  }
  Response* received = nullptr;
  if (xQueueReceive(responseQueue, &received, 0) != pdTRUE)
  {
    return false;
  }
  *response = std::move(*received);
  delete received;
  return true;
}
} // namespace HANet

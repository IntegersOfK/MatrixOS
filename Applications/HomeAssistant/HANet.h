#pragma once

#include "MatrixOS.h"

// Wi-Fi + Home Assistant REST client.
// Wi-Fi and the request worker start once and outlive the app, so relaunching the app never re-inits the stack.
// Requests are queued and run on the worker task; the UI never blocks on the network.
namespace HANet
{
enum class LinkState : uint8_t {
  Unconfigured, // No SSID in HAConfig.local.h
  Connecting,
  Connected,
  Failed, // Wi-Fi stack failed to start (see log); not retried until reboot
};

enum class Method : uint8_t {
  Get,
  Post,
};

struct Response {
  uint32_t tag = 0;
  int status = 0;    // HTTP status, 0 when the request never got a response
  int32_t error = 0; // esp_err_t from the transport (ESP_OK on success)
  string body;       // Truncated to MAX_RESPONSE_BODY bytes
};

static constexpr size_t MAX_RESPONSE_BODY = 2048;

// Idempotent. Safe to call from every app launch.
void Begin();

bool IsWifiConfigured();
bool HasToken();
LinkState GetLinkState();
uint8_t GetLastDisconnectReason(); // wifi_err_reason_t of the last drop, 0 if none

// Queue a request. Never blocks; returns false if the queue is full or Wi-Fi isn't configured.
bool Request(Method method, const string& path, const string& body, uint32_t tag);

// POST /api/services/<domain>/<service> with a JSON body such as {"entity_id":"light.hue_go"}
bool CallService(const string& domain, const string& service, const string& jsonBody, uint32_t tag);

// Non-blocking. Returns completed requests in order. Unread responses past the queue depth drop the oldest.
bool PollResponse(Response* response);
} // namespace HANet

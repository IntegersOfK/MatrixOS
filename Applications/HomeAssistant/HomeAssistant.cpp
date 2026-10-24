#include "HomeAssistant.h"
#include "HAConfig.h"
#include "HANet.h"
#include "UI/UI.h"

#include "esp_err.h"

static const char* TAG = "HomeAssistant";

static constexpr uint32_t TAG_API_CHECK = 1;
static constexpr uint32_t TAG_TOGGLE = 2;
static constexpr uint32_t RESULT_FLASH_MS = 1500;

static const Color COLOR_OK = Color(0x00FF40);
static const Color COLOR_HTTP_ERROR = Color(0xFF3000);    // HA answered with an error (401 = bad token)
static const Color COLOR_NETWORK_ERROR = Color(0xFF00A0); // No answer at all (Wi-Fi down, host unreachable, timeout)
static const Color COLOR_CONNECTING = Color(0xFFA000);

void HomeAssistant::Setup(const vector<string>& args) {
  (void)args;
  HANet::Begin();

  // Responses left over from a previous launch belong to probes that no longer exist
  HANet::Response stale;
  while (HANet::PollResponse(&stale))
  {
  }

  TestPage();
  Exit();
}

void HomeAssistant::Send(Probe& probe, bool queued) {
  probe.status = 0;
  probe.error = queued ? ESP_OK : ESP_FAIL;
  probe.state = queued ? Probe::State::Pending : Probe::State::Failed;
  probe.doneAt = queued ? 0 : MatrixOS::SYS::Millis();
}

void HomeAssistant::HandleResponses() {
  HANet::Response response;
  while (HANet::PollResponse(&response))
  {
    Probe* probe = response.tag == TAG_API_CHECK ? &apiProbe : response.tag == TAG_TOGGLE ? &toggleProbe : nullptr;
    if (probe == nullptr)
    {
      continue;
    }
    probe->status = response.status;
    probe->error = response.error;
    probe->state = (response.error == ESP_OK && response.status >= 200 && response.status < 300) ? Probe::State::Ok : Probe::State::Failed;
    probe->doneAt = MatrixOS::SYS::Millis();
    MLOGI(TAG, "Tag %lu -> %s", (unsigned long)response.tag, Describe(*probe).c_str());
  }
}

Color HomeAssistant::ProbeColor(const Probe& probe, Color idle) const {
  switch (probe.state)
  {
  case Probe::State::Pending:
    return Color::White;
  case Probe::State::Ok:
    return COLOR_OK;
  case Probe::State::Failed:
    return probe.status != 0 ? COLOR_HTTP_ERROR : COLOR_NETWORK_ERROR;
  default:
    return idle;
  }
}

string HomeAssistant::Describe(const Probe& probe) const {
  switch (probe.state)
  {
  case Probe::State::Idle:
    return "Not run";
  case Probe::State::Pending:
    return "Waiting";
  default:
    if (probe.status != 0)
    {
      return probe.status == 401 ? "HTTP 401 bad token" : "HTTP " + std::to_string(probe.status);
    }
    return string(esp_err_to_name(probe.error));
  }
}

string HomeAssistant::LinkDescription() const {
  switch (HANet::GetLinkState())
  {
  case HANet::LinkState::Unconfigured:
    return "No Wi-Fi config";
  case HANet::LinkState::Connecting:
    return HANet::GetLastDisconnectReason() ? "Wi-Fi reason " + std::to_string(HANet::GetLastDisconnectReason()) : "Connecting";
  case HANet::LinkState::Connected:
    return "Wi-Fi OK";
  default:
    return "Wi-Fi failed";
  }
}

void HomeAssistant::TestPage() {
  UI testPage("Home Assistant", info.color);

  // (0,0) Link status. Hold to scroll the detail.
  UIButton linkButton;
  linkButton.SetName("Wi-Fi");
  linkButton.SetColorFunc([]() -> Color {
    switch (HANet::GetLinkState())
    {
    case HANet::LinkState::Connected:
      return COLOR_OK;
    case HANet::LinkState::Connecting:
      return ColorEffects::ColorBreathLowBound(COLOR_CONNECTING, 32, 1500);
    case HANet::LinkState::Failed:
      return Color::Red;
    default:
      return Color(0x404040);
    }
  });
  linkButton.OnHold([this]() -> void { MatrixOS::UIUtility::TextScroll(LinkDescription(), Color::White); });
  testPage.AddUIComponent(linkButton, Point(0, 0));

  // (1,0) GET /api/ checks host + token: green 200, orange 401, pink no answer
  UIButton apiButton;
  apiButton.SetName("API check");
  apiButton.SetColorFunc([this]() -> Color { return ProbeColor(apiProbe, Color(0x0040FF)); });
  apiButton.OnPress([this]() -> void { Send(apiProbe, HANet::Request(HANet::Method::Get, "/api/", "", TAG_API_CHECK)); });
  apiButton.OnHold([this]() -> void { MatrixOS::UIUtility::TextScroll(Describe(apiProbe), Color::White); });
  testPage.AddUIComponent(apiButton, Point(1, 0));

  // Centre 2x2: toggle the test light. Result flashes, then back to idle.
  UIButton toggleButton;
  toggleButton.SetName("Toggle " HA_TEST_ENTITY);
  toggleButton.SetSize(Dimension(2, 2));
  toggleButton.SetColorFunc([this]() -> Color {
    bool flashOver = toggleProbe.state != Probe::State::Pending && MatrixOS::SYS::Millis() - toggleProbe.doneAt > RESULT_FLASH_MS;
    return flashOver ? Color(0xFFB060).Dim() : ProbeColor(toggleProbe, Color(0xFFB060).Dim());
  });
  toggleButton.OnPress([this]() -> void {
    Send(toggleProbe, HANet::CallService("light", "toggle", "{\"entity_id\":\"" HA_TEST_ENTITY "\"}", TAG_TOGGLE));
  });
  toggleButton.OnHold([this]() -> void { MatrixOS::UIUtility::TextScroll(Describe(toggleProbe), Color::White); });
  testPage.AddUIComponent(toggleButton, Point(3, 3));

  testPage.SetLoopFunc([this]() -> void { HandleResponses(); });
  testPage.Start();
}

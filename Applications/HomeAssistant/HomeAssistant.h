#pragma once

#include "MatrixOS.h"
#include "Application.h"

// Home Assistant controller. Phase 2: a network test page (link status, API/token check, toggle one test light).
// The real floor-plan layout replaces the test page in Phase 3.
class HomeAssistant : public Application {
public:
  inline static Application_Info info = {
      .name = "Home Assistant",
      .author = "AJ",
      .color = Color(0x18BCF2),
      .version = 1,
      .visibility = true,
  };

  void Setup(const vector<string>& args) override;

private:
  // Tracks one outstanding request so its key can show pending / ok / error
  struct Probe {
    enum class State : uint8_t {
      Idle,
      Pending,
      Ok,
      Failed,
    };
    State state = State::Idle;
    int status = 0;
    int32_t error = 0;
    uint32_t doneAt = 0;
  };

  Probe apiProbe;
  Probe toggleProbe;

  void TestPage();
  void Send(Probe& probe, bool queued);
  void HandleResponses();
  Color ProbeColor(const Probe& probe, Color idle) const;
  string Describe(const Probe& probe) const;
  string LinkDescription() const;
};

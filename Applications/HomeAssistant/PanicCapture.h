#pragma once

#include <cstdint>

// Diagnostic: the last panic, kept across the reboot in RTC memory.
namespace PanicCapture
{
static constexpr uint32_t MAX_DEPTH = 16;

struct Record {
  uint32_t magic;
  char reason[48];
  char abortDetails[96];
  int core;
  uint32_t pc;
  uint32_t excCause;
  uint32_t excVaddr;
  uint32_t backtrace[MAX_DEPTH];
  uint32_t depth;
};

// Copies the record from the previous panic, if any, and clears it.
bool Take(Record* out);
} // namespace PanicCapture

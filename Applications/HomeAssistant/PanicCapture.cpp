// Diagnostic: keep the last panic in RTC memory so it can be read back over the USB log after the reboot.
// The panic handler prints to UART0, which isn't wired out, and the flash core dump hasn't been landing.
// Hooked with -Wl,--wrap=esp_panic_handler (see CMakeLists.txt).
#include "PanicCapture.h"

#include "esp_attr.h"
#include "esp_cpu_utils.h"
#include "esp_debug_helpers.h"
#include "esp_private/panic_internal.h"
#include "xtensa_context.h"

#include <cstring>

extern "C" char* g_panic_abort_details;
extern "C" void __real_esp_panic_handler(panic_info_t* info);

static constexpr uint32_t RECORD_MAGIC = 0x48415043; // "HAPC"

static RTC_NOINIT_ATTR PanicCapture::Record record;

static void IRAM_ATTR CopyString(char* dest, size_t size, const char* src) {
  size_t i = 0;
  for (; src != nullptr && src[i] != '\0' && i + 1 < size; i++)
  {
    dest[i] = src[i];
  }
  dest[i] = '\0';
}

extern "C" void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t* info) {
  record.magic = 0;
  CopyString(record.reason, sizeof(record.reason), info->reason);
  CopyString(record.abortDetails, sizeof(record.abortDetails), g_panic_abort_details);
  record.core = info->core;
  record.pc = (uint32_t)info->addr;
  record.depth = 0;
  const XtExcFrame* frame = (const XtExcFrame*)info->frame;
  if (frame != nullptr)
  {
    record.excCause = frame->exccause;
    record.excVaddr = frame->excvaddr;
    esp_backtrace_frame_t walk = {.pc = (uint32_t)frame->pc, .sp = (uint32_t)frame->a1, .next_pc = (uint32_t)frame->a0, .exc_frame = frame};
    record.backtrace[record.depth++] = esp_cpu_process_stack_pc(walk.pc);
    while (record.depth < PanicCapture::MAX_DEPTH && walk.next_pc != 0 && esp_backtrace_get_next_frame(&walk))
    {
      record.backtrace[record.depth++] = esp_cpu_process_stack_pc(walk.pc);
    }
  }
  record.magic = RECORD_MAGIC;
  __real_esp_panic_handler(info);
}

namespace PanicCapture
{
bool Take(Record* out) {
  if (record.magic != RECORD_MAGIC)
  {
    return false;
  }
  *out = record;
  out->reason[sizeof(out->reason) - 1] = '\0';
  out->abortDetails[sizeof(out->abortDetails) - 1] = '\0';
  if (out->depth > MAX_DEPTH)
  {
    out->depth = MAX_DEPTH;
  }
  record.magic = 0;
  return true;
}
} // namespace PanicCapture

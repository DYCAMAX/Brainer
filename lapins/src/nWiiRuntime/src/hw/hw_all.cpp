#include "runtime/hw/hw.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace nwii::runtime::hw {
extern void register_cp(MMIODispatcher &dispatcher);

// Removing the hw/ layer means proving, block by block, that the game does not
// actually need it -- most of these are shadowed by SDK functions that are
// already served natively at the seam (DVD*, PAD*, AX*, VI*). NWII_NOHW takes a
// comma-separated list of block names to leave unregistered; an unregistered
// range falls through to the dispatcher's unhandled path, which logs every
// access and returns 0, so one run says exactly what that block was carrying.
// NWII_NOHW=all disables every block at once.
//
// Measured on Mario Party 7: si, exi and mi carried nothing -- the game runs
// unchanged without them, and input does not go through SI at all because
// PADRead is served at the seam straight from InputManager. Those three are
// gone. cp, pi, pe, vi, dsp, ai and di are still load-bearing: the SDK reaches
// their registers from inside its own routines, so they can only be dropped
// once the SDK functions above them are served at the seam.
static bool skipped(const char *name) {
  static const char *list = std::getenv("NWII_NOHW");
  if (!list) return false;
  if (std::strcmp(list, "all") == 0) return true;
  const size_t n = std::strlen(name);
  for (const char *p = list; *p;) {
    const char *e = std::strchr(p, ',');
    size_t len = e ? (size_t)(e - p) : std::strlen(p);
    if (len == n && std::strncmp(p, name, n) == 0) return true;
    if (!e) break;
    p = e + 1;
  }
  return false;
}

void register_all_hw(MMIODispatcher &dispatcher) {
#define NWII_HW_BLOCK(name)                                                    \
  do {                                                                         \
    if (skipped(#name)) {                                                      \
      std::cerr << "[hw] block " #name " left unregistered\n";                 \
    } else {                                                                   \
      register_##name(dispatcher);                                             \
    }                                                                          \
  } while (0)

  NWII_HW_BLOCK(cp);
  NWII_HW_BLOCK(pi);
  NWII_HW_BLOCK(pe);
  NWII_HW_BLOCK(vi);
  NWII_HW_BLOCK(dsp);
  NWII_HW_BLOCK(ai);
  NWII_HW_BLOCK(di);
  NWII_HW_BLOCK(ipc);
#undef NWII_HW_BLOCK
}
}

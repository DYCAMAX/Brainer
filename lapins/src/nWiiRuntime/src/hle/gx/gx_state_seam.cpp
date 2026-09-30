// GX render state served at the seam.
//
// Full-register setters emit the exact BP write the SDK would have produced
// (framing: 0x61, u8 register, 24-bit value). Setters that own only part of a
// register must NOT merge here: the shadow state they would merge against is
// updated by the parser, which runs up to a frame behind this call. Those emit
// a 0x78 RMW marker instead -- mask + value -- and the parser performs the
// merge at the marker's position in the stream, where the base is current.

#include "runtime/cpu_context.h"
#include "runtime/gx_state.h"

using nwii::runtime::CPUContext;

namespace {

void BPWrite(uint8_t reg, uint32_t val) {
  nwii::runtime::GX_WGPIPE_Write8(0x61);
  nwii::runtime::GX_WGPIPE_Write8(reg);
  nwii::runtime::GX_WGPIPE_Write8((val >> 16) & 0xFF);
  nwii::runtime::GX_WGPIPE_Write8((val >> 8) & 0xFF);
  nwii::runtime::GX_WGPIPE_Write8(val & 0xFF);
}

// kind 0 = BP register, 1 = CP register.
void RMWWrite(uint8_t kind, uint8_t reg, uint32_t mask, uint32_t val) {
  nwii::runtime::GX_WGPIPE_Write8(0x78);
  nwii::runtime::GX_WGPIPE_Write8(kind);
  nwii::runtime::GX_WGPIPE_Write8(reg);
  nwii::runtime::GX_WGPIPE_Write32(mask);
  nwii::runtime::GX_WGPIPE_Write32(val);
}

} // namespace

extern "C" {

// void GXSetCullMode(GXCullMode mode)
// API order is none/front/back/all; GEN_MODE stores none/back/front/all, so
// 1 and 2 swap on the way in -- the same table libogc uses.
void GXSetCullMode(CPUContext &ctx) {
  static const uint8_t to_hw[4] = {0, 2, 1, 3};
  const uint32_t m = to_hw[ctx.gpr[3] & 3];
  RMWWrite(0, 0x00, 3u << 14, m << 14);
}

// void GXSetNumTevStages(u8 num) -- GEN_MODE bits 10-13 hold num-1.
void GXSetNumTevStages(CPUContext &ctx) {
  const uint32_t n = (ctx.gpr[3] - 1) & 0xF;
  RMWWrite(0, 0x00, 0xFu << 10, n << 10);
}

// void GXSetBlendMode(GXBlendMode type, GXBlendFactor src, GXBlendFactor dst,
//                     GXLogicOp op) -- BP 0x41 (PE_CMODE0). Bits 2-4 (dither,
// colour/alpha update) belong to other setters and stay untouched.
void GXSetBlendMode(CPUContext &ctx) {
  const uint32_t type = ctx.gpr[3], src = ctx.gpr[4] & 7, dst = ctx.gpr[5] & 7,
                 op = ctx.gpr[6] & 0xF;
  uint32_t v = 0;
  if (type == 1 || type == 3) v |= 1;        // blend / subtract enable
  if (type == 2) v |= 2;                     // logic op
  v |= dst << 5;
  v |= src << 8;
  if (type == 3) v |= 1u << 11;              // subtract
  v |= op << 12;
  RMWWrite(0, 0x41, 0xFFE3u, v);
}

// void GXSetZMode(GXBool compare_enable, GXCompare func, GXBool update_enable)
void GXSetZMode(CPUContext &ctx) {
  const uint32_t v =
      (ctx.gpr[3] & 1) | ((ctx.gpr[4] & 7) << 1) | ((ctx.gpr[5] & 1) << 4);
  BPWrite(0x40, v);
}

// void GXSetZCompLoc(GXBool before_tex) -- BP 0x43 bit 6.
void GXSetZCompLoc(CPUContext &ctx) {
  RMWWrite(0, 0x43, 1u << 6, (ctx.gpr[3] & 1) << 6);
}

// void GXSetAlphaCompare(GXCompare c0, u8 ref0, GXAlphaOp op, GXCompare c1,
//                        u8 ref1) -- BP 0xF3, full register.
void GXSetAlphaCompare(CPUContext &ctx) {
  const uint32_t v = (ctx.gpr[4] & 0xFF) | ((ctx.gpr[7] & 0xFF) << 8) |
                     ((ctx.gpr[3] & 7) << 16) | ((ctx.gpr[6] & 7) << 19) |
                     ((ctx.gpr[5] & 3) << 22);
  BPWrite(0xF3, v);
}

// void GXSetDstAlpha(GXBool enable, u8 a) -- BP 0x42, full register.
void GXSetDstAlpha(CPUContext &ctx) {
  BPWrite(0x42, (ctx.gpr[4] & 0xFF) | ((ctx.gpr[3] & 1) << 8));
}

// void GXSetScissor(u32 x, u32 y, u32 wd, u32 ht)
// Scissor coordinates carry the hardware's 342 pixel origin offset.
void GXSetScissor(CPUContext &ctx) {
  const uint32_t xo = ctx.gpr[3] + 342, yo = ctx.gpr[4] + 342;
  const uint32_t xe = xo + ctx.gpr[5] - 1, ye = yo + ctx.gpr[6] - 1;
  BPWrite(0x20, (yo & 0x7FF) | ((xo & 0x7FF) << 12));
  BPWrite(0x21, (ye & 0x7FF) | ((xe & 0x7FF) << 12));
}

} // extern "C"

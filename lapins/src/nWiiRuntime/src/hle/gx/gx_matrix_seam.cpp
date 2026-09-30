// GX matrix loads served at the seam.
//
// These are real SDK function calls, so we get the matrix pointer, the slot and
// the moment for free -- none of it has to be reconstructed from the register
// stream. What we must not do is write g_state.posMatrices straight away: the
// FIFO is parsed in batches, so a matrix applied at call time would land
// against the wrong draws. Instead each handler emits the exact FIFO bytes the
// hardware SDK would have written, from known-good arguments. Ordering is then
// correct by construction, and the parser stays unchanged.
//
// Framing (see FifoParser::Parse): XF load is 0x10, u16 count-1, u16 address,
// then count big-endian words; CP write is 0x08, u8 register, u32 value.

#include "runtime/cpu_context.h"
#include "runtime/gx_state.h"

using nwii::runtime::CPUContext;

namespace {

void XFLoad(CPUContext &ctx, uint16_t addr, uint16_t count, uint32_t src) {
  nwii::runtime::GX_WGPIPE_Write8(0x10);
  nwii::runtime::GX_WGPIPE_Write16(count - 1);
  nwii::runtime::GX_WGPIPE_Write16(addr);
  for (uint16_t i = 0; i < count; ++i)
    nwii::runtime::GX_WGPIPE_Write32(ctx.mmu.read32(src + i * 4));
}

} // namespace

extern "C" {

// void GXLoadPosMtxImm(const Mtx mtx, u32 id) -- 3x4, XF matrix memory at id*4.
void GXLoadPosMtxImm(CPUContext &ctx) {
  XFLoad(ctx, (uint16_t)((ctx.gpr[4] & 0x3F) * 4), 12, ctx.gpr[3]);
}

// void GXLoadNrmMtxImm(const Mtx mtx, u32 id) -- the upper 3x3 of a 3x4 source,
// into the normal matrix bank at 0x0400 + 3*(id&31).
void GXLoadNrmMtxImm(CPUContext &ctx) {
  const uint32_t src = ctx.gpr[3];
  const uint16_t addr = (uint16_t)(0x0400 + 3 * (ctx.gpr[4] & 31));
  nwii::runtime::GX_WGPIPE_Write8(0x10);
  nwii::runtime::GX_WGPIPE_Write16(9 - 1);
  nwii::runtime::GX_WGPIPE_Write16(addr);
  for (int row = 0; row < 3; ++row)
    for (int col = 0; col < 3; ++col)
      nwii::runtime::GX_WGPIPE_Write32(ctx.mmu.read32(src + (row * 4 + col) * 4));
}

// void GXLoadTexMtxImm(const Mtx mtx, u32 id, GXTexMtxType type)
// type 0 = 3x4 (12 words), 1 = 2x4 (8). Dual-transform slots start at 64 and
// live in their own bank at 0x0500.
void GXLoadTexMtxImm(CPUContext &ctx) {
  const uint32_t id = ctx.gpr[4];
  const uint16_t addr = (uint16_t)(id >= 64 ? 0x0500 + (id - 64) * 4 : id * 4);
  XFLoad(ctx, addr, ctx.gpr[5] == 0 ? 12 : 8, ctx.gpr[3]);
}

// void GXSetCurrentMtx(u32 id) -- CP matrix index A, position/normal in bits
// 0-5. The texture indices in the same register must survive untouched, and
// the merge must happen in the parser (0x78 marker), not here: the call-time
// shadow of cp[0x30] lags the stream by up to a frame.
void GXSetCurrentMtx(CPUContext &ctx) {
  nwii::runtime::GX_WGPIPE_Write8(0x78);
  nwii::runtime::GX_WGPIPE_Write8(1);
  nwii::runtime::GX_WGPIPE_Write8(0x30);
  nwii::runtime::GX_WGPIPE_Write32(0x3Fu);
  nwii::runtime::GX_WGPIPE_Write32(ctx.gpr[3] & 0x3Fu);
}

} // extern "C"

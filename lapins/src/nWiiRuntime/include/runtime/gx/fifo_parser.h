#pragma once
#include <vector>
#include <cstdint>
#include "runtime/gx_command.h"
#include "runtime/gx_state.h"

namespace nwii::runtime::gx {

void ApplyBPRegister(uint8_t reg, uint32_t val);

// Drain profiling counters (defined in fifo_parser.cpp, reported by gx.cpp).
extern uint64_t g_prof_draws, g_prof_draw_bytes, g_prof_dl_calls,
    g_prof_dl_bytes, g_prof_cmds, g_prof_unknown, g_prof_dl_us, g_prof_snap_us, g_prof_dl_loadindx, g_prof_dl_hits;


// Parse-time snapshot of everything needed to decode a draw's raw vertex
// bytes later (after frame-skip decides the draw survives).
struct DrawRaw {
    VATSlot vat;
    uint32_t arrayBase[16];
    uint32_t arrayStride[16];
    uint8_t defPosMtxIdx;
    uint8_t defTexMtxIdx[8];
    bool need_normal;   // lighting enabled at snapshot time
    // Position/normal matrix captured at parse time for this draw. GX skins by
    // swapping the matrix in a slot between draws; the renderer applies loads
    // late, so a live lookup gives the wrong (last-written) matrix. Snapshot
    // makes the transform use the matrix that was current for THIS draw.
    bool have_snap_mtx = false;
    float snap_pos_mtx[12];
    float snap_nrm_mtx[9];
    // Skinned draws pick a matrix per vertex, so one matrix is not enough --
    // the whole bank has to be frozen at this draw's stream position. Empty for
    // ordinary draws, so nothing is paid for them.
    std::vector<float> snap_pos_bank;   // 256 floats = 64 slots of 3x4
    std::vector<float> snap_nrm_bank;   // 96 floats  = 32 slots of 3x3
    uint16_t count;
    std::vector<uint8_t> bytes;
};

class FifoParser {
public:


    static void Parse(std::vector<uint8_t>& fifo, std::vector<GXCommand>& commands);

    // Decode a deferred draw's raw bytes into cmd.vertices using the state
    // snapshotted at parse time. No-op if already decoded or not a draw.
    static void DecodeDraw(GXCommand& cmd);
};

} 

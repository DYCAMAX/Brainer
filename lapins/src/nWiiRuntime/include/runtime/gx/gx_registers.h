// SPDX-License-Identifier: MIT
// Copyright (c) 2026 NWiiRecomp Contributors
#pragma once
#include <cstdint>
namespace nwii::runtime::gx {
enum class TevColorArg : uint32_t {
  ColorPrev = 0,
  AlphaPrev = 1,
  ColorC0 = 2,
  AlphaC0 = 3,
  ColorC1 = 4,
  AlphaC1 = 5,
  ColorC2 = 6,
  AlphaC2 = 7,
  TexColor = 8,
  TexAlpha = 9,
  RasColor = 10,
  RasAlpha = 11,
  One = 12,
  Half = 13,
  Konst = 14,
  Zero = 15
};
enum class TevAlphaArg : uint32_t {
  AlphaPrev = 0,
  AlphaC0 = 1,
  AlphaC1 = 2,
  AlphaC2 = 3,
  TexAlpha = 4,
  RasAlpha = 5,
  Konst = 6,
  Zero = 7
};
enum class TevOp : uint32_t { Add = 0, Sub = 1 };
enum class TevBias : uint32_t {
  Zero = 0,
  AddHalf = 1,
  SubHalf = 2,
  Compare = 3
};
enum class TevScale : uint32_t {
  Scale1 = 0,
  Scale2 = 1,
  Scale4 = 2,
  Divide2 = 3
};
enum class TevOutput : uint32_t { Prev = 0, Reg0 = 1, Reg1 = 2, Reg2 = 3 };
struct TevColorEnv {
  TevColorArg d;
  TevColorArg c;
  TevColorArg b;
  TevColorArg a;
  TevBias bias;
  TevOp op;
  bool clamp;
  TevScale scale;
  TevOutput dest;
};
struct TevAlphaEnv {
  uint8_t rswap;
  uint8_t tswap;
  TevAlphaArg d;
  TevAlphaArg c;
  TevAlphaArg b;
  TevAlphaArg a;
  TevBias bias;
  TevOp op;
  bool clamp;
  TevScale scale;
  TevOutput dest;
};
enum class RasColorChan : uint32_t {
  Color0 = 0,
  Color1 = 1,
  Color0A = 2,
  Color1A = 3,
  ColorZero = 4,
  BumpAlpha = 5,
  NormalizedBumpAlpha = 6,
  Zero = 7
};
struct TevOrder {
  uint32_t texmap;
  uint32_t texcoord;
  bool enable_tex;
  RasColorChan colorchan;
};
enum class CompareMode : uint32_t {
    Never = 0, Less = 1, Equal = 2, LEqual = 3,
    Greater = 4, NEqual = 5, GEqual = 6, Always = 7
};
enum class XFColorSource : uint32_t {
    Register = 0, Vertex = 1
};
enum class XFDiffuseFunc : uint32_t {
    None = 0, Sign = 1, Clamp = 2
};
enum class XFAttnFunc : uint32_t {
    Spec = 0, Spot = 1, None = 2
};
enum class AlphaTestOp : uint32_t {
    And = 0, Or = 1, Xor = 2, Xnor = 3
};
enum class KonstSel : uint32_t {
    K1 = 0, K1_8 = 1, K1_4 = 2, K1_2 = 3,
    K2 = 4, K2_8 = 5, K2_4 = 6, K2_2 = 7,
    K3 = 8, K3_8 = 9, K3_4 = 10, K3_2 = 11,
    K4 = 12, K4_8 = 13, K4_4 = 14, K4_2 = 15,
    K1_R = 16, K2_R = 17, K3_R = 18, K4_R = 19,
    K1_G = 20, K2_G = 21, K3_G = 22, K4_G = 23,
    K1_B = 24, K2_B = 25, K3_B = 26, K4_B = 27,
    K1_A = 28, K2_A = 29, K3_A = 30, K4_A = 31
};
enum class ColorChannel : uint32_t {
    Red = 0, Green = 1, Blue = 2, Alpha = 3
};
struct ZMode {
    bool test_enable;
    CompareMode func;
    bool update_enable;
};
struct AlphaTest {
    uint8_t ref0;
    uint8_t ref1;
    CompareMode comp0;
    CompareMode comp1;
    AlphaTestOp logic;
};
struct XFLightingChannel {
    XFColorSource matsource;
    bool enablelighting;
    XFColorSource ambsource;
    XFDiffuseFunc diffusefunc;
    XFAttnFunc attnfunc;
    uint8_t light_mask;
};
struct TevKSel {
    ColorChannel swap_rb;
    ColorChannel swap_ga;
    KonstSel kcsel_even;
    KonstSel kasel_even;
    KonstSel kcsel_odd;
    KonstSel kasel_odd;
};
struct TevSwapTable {
    ColorChannel r;
    ColorChannel g;
    ColorChannel b;
    ColorChannel a;
};
struct TevStageIndirect {
    uint8_t bt;
    uint8_t fmt;
    uint8_t bias;
    uint8_t bs;
    uint8_t matrix_index;
    uint8_t matrix_id;
    uint8_t sw;
    uint8_t tw;
    bool lb_utclod;
    bool fb_addprev;
};
struct TevStageEnv {
    TevColorEnv color;
    TevAlphaEnv alpha;
    TevOrder order;
    TevKSel ksel;
    TevStageIndirect indirect;
};
inline TevColorEnv ExtractTevColorEnv(uint32_t val) {
  return {static_cast<TevColorArg>((val >> 0) & 0xF),
          static_cast<TevColorArg>((val >> 4) & 0xF),
          static_cast<TevColorArg>((val >> 8) & 0xF),
          static_cast<TevColorArg>((val >> 12) & 0xF),
          static_cast<TevBias>((val >> 16) & 0x3),
          static_cast<TevOp>((val >> 18) & 0x1),
          ((val >> 19) & 0x1) != 0,
          static_cast<TevScale>((val >> 20) & 0x3),
          static_cast<TevOutput>((val >> 22) & 0x3)};
}
inline TevAlphaEnv ExtractTevAlphaEnv(uint32_t val) {
  return {static_cast<uint8_t>((val >> 0) & 0x3),
          static_cast<uint8_t>((val >> 2) & 0x3),
          static_cast<TevAlphaArg>((val >> 4) & 0x7),
          static_cast<TevAlphaArg>((val >> 7) & 0x7),
          static_cast<TevAlphaArg>((val >> 10) & 0x7),
          static_cast<TevAlphaArg>((val >> 13) & 0x7),
          static_cast<TevBias>((val >> 16) & 0x3),
          static_cast<TevOp>((val >> 18) & 0x1),
          ((val >> 19) & 0x1) != 0,
          static_cast<TevScale>((val >> 20) & 0x3),
          static_cast<TevOutput>((val >> 22) & 0x3)};
}
inline TevOrder ExtractTevOrder(uint32_t val, bool is_odd) {
    if (!is_odd) {
        return {
            (val >> 0) & 0x7,
            (val >> 3) & 0x7,
            ((val >> 6) & 0x1) != 0,
            static_cast<RasColorChan>((val >> 7) & 0x7)
        };
    } else {
        return {
            (val >> 12) & 0x7,
            (val >> 15) & 0x7,
            ((val >> 18) & 0x1) != 0,
            static_cast<RasColorChan>((val >> 19) & 0x7)
        };
    }
}
inline ZMode ExtractZMode(uint32_t val) {
    return {
        ((val >> 0) & 0x1) != 0,
        static_cast<CompareMode>((val >> 1) & 0x7),
        ((val >> 4) & 0x1) != 0
    };
}
inline AlphaTest ExtractAlphaTest(uint32_t val) {
    return {
        static_cast<uint8_t>((val >> 0) & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF),
        static_cast<CompareMode>((val >> 16) & 0x7),
        static_cast<CompareMode>((val >> 19) & 0x7),
        static_cast<AlphaTestOp>((val >> 22) & 0x3)
    };
}
inline TevKSel ExtractTevKSel(uint32_t val) {
    return {
        static_cast<ColorChannel>((val >> 0) & 0x3),
        static_cast<ColorChannel>((val >> 2) & 0x3),
        static_cast<KonstSel>((val >> 4) & 0x1F),
        static_cast<KonstSel>((val >> 9) & 0x1F),
        static_cast<KonstSel>((val >> 14) & 0x1F),
        static_cast<KonstSel>((val >> 19) & 0x1F)
    };
}
inline TevSwapTable ExtractTevSwapTable(uint32_t val_rg, uint32_t val_ba) {
    TevKSel rg = ExtractTevKSel(val_rg);
    TevKSel ba = ExtractTevKSel(val_ba);
    return { rg.swap_rb, rg.swap_ga, ba.swap_rb, ba.swap_ga };
}
inline TevStageIndirect ExtractTevStageIndirect(uint32_t val) {
    return {
        static_cast<uint8_t>((val >> 0) & 0x3),
        static_cast<uint8_t>((val >> 2) & 0x3),
        static_cast<uint8_t>((val >> 4) & 0x7),
        static_cast<uint8_t>((val >> 7) & 0x3),
        static_cast<uint8_t>((val >> 9) & 0x3),
        static_cast<uint8_t>((val >> 11) & 0x3),
        static_cast<uint8_t>((val >> 13) & 0x7),
        static_cast<uint8_t>((val >> 16) & 0x7),
        ((val >> 19) & 0x1) != 0,
        ((val >> 20) & 0x1) != 0
    };
}
inline XFLightingChannel ExtractXFLightingChannel(uint32_t val) {
    uint8_t lm03 = (val >> 2) & 0xF;
    uint8_t lm47 = (val >> 11) & 0xF;
    return {
        static_cast<XFColorSource>((val >> 0) & 0x1),
        ((val >> 1) & 0x1) != 0,
        static_cast<XFColorSource>((val >> 6) & 0x1),
        static_cast<XFDiffuseFunc>((val >> 7) & 0x3),
        static_cast<XFAttnFunc>((val >> 9) & 0x3),
        static_cast<uint8_t>(lm03 | (lm47 << 4))
    };
}
} 
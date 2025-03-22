// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
//
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// For more information, please refer to <https://unlicense.org/>
#pragma once

#include "reshade_api.hpp"
//#include "reshade_api_display.hpp"
#include <com_ptr.hpp>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <xstring>
#include <algorithm>
#include <memory>
#include <io.h>

#include <wincodec.h>

#pragma comment (lib, "windowscodecs.lib")

// Use AVX for SIMD fp16 to fp32 conversion
#pragma push_macro("_XM_F16C_INTRINSICS_")
#if (defined _M_IX86) || (defined _M_X64)
#undef  _XM_F16C_INTRINSICS_
#define _XM_F16C_INTRINSICS_
#endif

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXPackedVector.inl>

namespace sk_hdr_png
{
  using format = reshade::api::format;

#define DeclareUint32(x,y) uint32_t x = SetUint32((x),(y))
#if (defined _M_IX86) || (defined _M_X64)
  uint32_t GetUint32 (uint32_t  x)             noexcept { return _byteswap_ulong (x);           };
  uint32_t SetUint32 (uint32_t& x, uint32_t y) noexcept {    x = _byteswap_ulong (y); return x; };
#else
  uint32_t GetUint32 (uint32_t  x)             noexcept { return x;           };
  uint32_t SetUint32 (uint32_t& x, uint32_t y) noexcept {    x = y; return x; };
#endif

  struct cHRM_Payload
  {
    DeclareUint32 (white_x, 31270);
    DeclareUint32 (white_y, 32900);
    DeclareUint32 (red_x,   70800);
    DeclareUint32 (red_y,   29200);
    DeclareUint32 (green_x, 17000);
    DeclareUint32 (green_y, 79700);
    DeclareUint32 (blue_x,  13100);
    DeclareUint32 (blue_y,   4600);
  };

  struct sBIT_Payload
  {
    uint8_t red_bits   = 10; // May be up to 16, for scRGB data
    uint8_t green_bits = 10; // May be up to 16, for scRGB data
    uint8_t blue_bits  = 10; // May be up to 16, for scRGB data
  };

  struct mDCv_Payload
  {
    struct {
      DeclareUint32 (red_x,   35400); // 0.708 / 0.00002
      DeclareUint32 (red_y,   14600); // 0.292 / 0.00002
      DeclareUint32 (green_x,  8500); // 0.17  / 0.00002
      DeclareUint32 (green_y, 39850); // 0.797 / 0.00002
      DeclareUint32 (blue_x,   6550); // 0.131 / 0.00002
      DeclareUint32 (blue_y,   2300); // 0.046 / 0.00002
    } primaries;

    struct {
      DeclareUint32 (x, 15635); // 0.3127 / 0.00002
      DeclareUint32 (y, 16450); // 0.3290 / 0.00002
    } white_point;

    // The only real data we need to fill-in
    struct {
      DeclareUint32 (maximum, 10000000); // 1000.0 cd/m^2
      DeclareUint32 (minimum, 1);        // 0.0001 cd/m^2
    } luminance;
  };

  struct cLLi_Payload
  {
    DeclareUint32 (max_cll,  10000000); // 1000 / 0.0001
    DeclareUint32 (max_fall,  2500000); //  250 / 0.0001
  };

  //
  // ICC Profile for tonemapping comes courtesy of ledoge
  //
  //   https://github.com/ledoge/jxr_to_png
  //
  struct iCCP_Payload
  {
    char          profile_name [20]   = "RGB_D65_202_Rel_PeQ";
    uint8_t       compression_type    = 0;// (PNG_COMPRESSION_TYPE_DEFAULT)
    unsigned char profile_data [2178] =
    {
      0x78, 0x9C, 0xED, 0x97, 0x79, 0x58, 0x13, 0x67, 0x1E, 0xC7, 0x47, 0x50,
      0x59, 0x95, 0x2A, 0xAC, 0xED, 0xB6, 0x8B, 0xA8, 0x54, 0x20, 0x20, 0x42,
      0xE5, 0xF4, 0x00, 0x51, 0x40, 0x05, 0xAF, 0x6A, 0x04, 0x51, 0x6E, 0x84,
      0x70, 0xAF, 0x20, 0x24, 0xDC, 0x87, 0x0C, 0xA8, 0x88, 0x20, 0x09, 0x90,
      0x04, 0x12, 0x24, 0x24, 0x90, 0x03, 0x82, 0xA0, 0x41, 0x08, 0x24, 0x41,
      0x2E, 0x21, 0x01, 0x12, 0x83, 0x4A, 0x10, 0xA9, 0x56, 0xB7, 0x8A, 0xE0,
      0xAD, 0x21, 0xE0, 0xB1, 0x6B, 0x31, 0x3B, 0x49, 0x74, 0x09, 0x6D, 0xD7,
      0x3E, 0xCF, 0x3E, 0xFD, 0xAF, 0x4E, 0x3E, 0xF3, 0xBC, 0xBF, 0x79, 0xBF,
      0xEF, 0xBC, 0x33, 0x9F, 0xC9, 0xFC, 0x31, 0x2F, 0x00, 0xE8, 0xBC, 0x8D,
      0x4A, 0x3E, 0x62, 0x30, 0xD7, 0x09, 0x00, 0xA2, 0x63, 0xE2, 0x91, 0xEE,
      0x6E, 0x2E, 0x06, 0x7B, 0x82, 0x82, 0x0D, 0xB4, 0x46, 0x01, 0x6D, 0x60,
      0x0E, 0xA0, 0xDC, 0x82, 0x10, 0xA8, 0x58, 0x67, 0x38, 0x7C, 0x8F, 0xEA,
      0xE8, 0x57, 0x1B, 0x34, 0xEA, 0xF5, 0xB0, 0x6A, 0xAC, 0xC4, 0x42, 0x31,
      0xD7, 0xF2, 0x84, 0x9D, 0x68, 0xBD, 0xA9, 0xD6, 0x43, 0xEB, 0x16, 0xE5,
      0xBD, 0xFC, 0xC6, 0xD2, 0xFC, 0xF8, 0xFF, 0x38, 0xEF, 0xE3, 0xB6, 0x30,
      0x24, 0x14, 0x85, 0x80, 0xDA, 0x9F, 0xA1, 0x7D, 0x1B, 0x22, 0x16, 0x19,
      0x0F, 0x4D, 0xE9, 0x04, 0xD5, 0x46, 0x49, 0xF1, 0xB1, 0x8A, 0x3A, 0x04,
      0xAA, 0xBF, 0x44, 0x44, 0x04, 0x41, 0xED, 0x9C, 0x64, 0xA8, 0x36, 0x47,
      0x44, 0x22, 0x62, 0xA1, 0x9A, 0x06, 0xD5, 0xDA, 0x48, 0x2F, 0x6F, 0x1F,
      0xA8, 0x66, 0x29, 0xC6, 0x84, 0xAB, 0xEA, 0x1E, 0x45, 0x1D, 0xAC, 0xAA,
      0x47, 0x14, 0xB5, 0xB3, 0xB5, 0x8B, 0x25, 0x54, 0x3F, 0x03, 0x80, 0xC5,
      0x97, 0x5C, 0xAC, 0x9D, 0xA1, 0x5A, 0xA7, 0x06, 0xEA, 0x87, 0x47, 0x1F,
      0x49, 0x50, 0x5C, 0xF7, 0x83, 0x03, 0xA0, 0x1D, 0x1A, 0xE3, 0xE9, 0x01,
      0xB5, 0x30, 0x68, 0xD7, 0x07, 0xDC, 0x01, 0x37, 0xC0, 0x05, 0x08, 0x04,
      0xB6, 0x01, 0xEB, 0x00, 0x3B, 0xA8, 0xB5, 0x06, 0x2C, 0xA1, 0x3D, 0x10,
      0xEA, 0x0F, 0x05, 0x8E, 0x40, 0x2D, 0x1C, 0x6A, 0xF7, 0x43, 0xCF, 0xEC,
      0xB7, 0xE7, 0x98, 0xAF, 0x9C, 0x63, 0x2B, 0xF4, 0x83, 0xAE, 0x06, 0xDD,
      0x8A, 0x81, 0x6A, 0xC8, 0xCC, 0x73, 0x42, 0x85, 0xD9, 0x58, 0xAB, 0xCE,
      0xD2, 0x86, 0x5C, 0xE7, 0xDD, 0x91, 0xCB, 0x27, 0xCD, 0x00, 0x40, 0xAB,
      0x18, 0x00, 0xA6, 0x0B, 0xE5, 0xF2, 0x77, 0x54, 0xB9, 0x7C, 0x9A, 0x0A,
      0x00, 0x9A, 0xB7, 0x01, 0xA0, 0x33, 0x4B, 0xE5, 0x0B, 0x00, 0x0B, 0x74,
      0x80, 0x39, 0x33, 0x73, 0xD5, 0x45, 0x00, 0x80, 0xDB, 0x51, 0xB9, 0x5C,
      0x9E, 0x3D, 0xD3, 0x67, 0x16, 0x09, 0xF5, 0x8F, 0x42, 0xF3, 0xD4, 0xCF,
      0xF4, 0x19, 0x68, 0x01, 0xC0, 0xA2, 0xF3, 0x00, 0x70, 0x65, 0x69, 0x74,
      0x58, 0xBC, 0x95, 0xA2, 0x47, 0x53, 0x73, 0x81, 0xEA, 0x6E, 0x7F, 0xF1,
      0x2F, 0xFE, 0xEA, 0x78, 0x8E, 0x86, 0xE6, 0xDC, 0x79, 0xF3, 0xB5, 0xFE,
      0xB2, 0x60, 0xE1, 0x22, 0xED, 0x2F, 0x16, 0x2F, 0xD1, 0xD1, 0xFD, 0xEB,
      0xD2, 0x2F, 0xBF, 0xFA, 0xDB, 0xD7, 0xDF, 0xFC, 0x5D, 0x6F, 0x99, 0xFE,
      0xF2, 0x15, 0x2B, 0x0D, 0xBE, 0x5D, 0x65, 0x68, 0x64, 0x0C, 0x33, 0x31,
      0x5D, 0x6D, 0xB6, 0xC6, 0xDC, 0xE2, 0xBB, 0xB5, 0x96, 0x56, 0xD6, 0x36,
      0xB6, 0x76, 0xEB, 0xD6, 0x6F, 0xD8, 0x68, 0xEF, 0xB0, 0xC9, 0x71, 0xF3,
      0x16, 0x27, 0x67, 0x97, 0xAD, 0xDB, 0xB6, 0xBB, 0xBA, 0xED, 0xD8, 0xB9,
      0x6B, 0xF7, 0x9E, 0xEF, 0xF7, 0xEE, 0x83, 0xEF, 0x77, 0xF7, 0x38, 0xE0,
      0x79, 0xF0, 0x10, 0x74, 0x6F, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x87, 0x83,
      0x82, 0x11, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xFF, 0x38, 0x12,
      0x1D, 0x73, 0x34, 0x36, 0x0E, 0x89, 0x8A, 0x4F, 0x48, 0x4C, 0x4A, 0x4E,
      0x49, 0x4D, 0x4B, 0xCF, 0x38, 0x96, 0x09, 0x66, 0x65, 0x1F, 0x3F, 0x71,
      0x32, 0xE7, 0x54, 0xEE, 0xE9, 0xBC, 0xFC, 0x33, 0x05, 0x68, 0x4C, 0x61,
      0x51, 0x31, 0x16, 0x87, 0x2F, 0x29, 0x25, 0x10, 0xCB, 0xCE, 0x96, 0x93,
      0x2A, 0xC8, 0x94, 0xCA, 0x2A, 0x2A, 0x8D, 0xCE, 0xA8, 0xAE, 0x61, 0xD6,
      0x9E, 0xAB, 0xAB, 0x3F, 0x7F, 0x81, 0xD5, 0x70, 0xB1, 0xB1, 0x89, 0xDD,
      0xDC, 0xC2, 0xE1, 0xF2, 0x5A, 0x2F, 0xB5, 0xB5, 0x77, 0x74, 0x76, 0x5D,
      0xEE, 0xEE, 0xE1, 0x0B, 0x7A, 0xFB, 0xFA, 0x85, 0xA2, 0x2B, 0xE2, 0x81,
      0xAB, 0xD7, 0xAE, 0x0F, 0x4A, 0x86, 0x6E, 0x0C, 0xDF, 0x1C, 0xF9, 0xE1,
      0xD6, 0xED, 0x1F, 0xEF, 0xDC, 0xFD, 0xE7, 0x4F, 0xF7, 0xEE, 0x8F, 0x3E,
      0x18, 0x1B, 0x7F, 0xF8, 0xE8, 0xF1, 0x93, 0xA7, 0xCF, 0x9E, 0xBF, 0x78,
      0x29, 0x9D, 0x90, 0x4D, 0x4E, 0xBD, 0x7A, 0xFD, 0xE6, 0xED, 0xBF, 0xFE,
      0xFD, 0xEE, 0xE7, 0xE9, 0xF7, 0xF2, 0xCF, 0xFE, 0x7F, 0x72, 0x7F, 0x10,
      0x04, 0xB2, 0x32, 0x34, 0x4E, 0x85, 0x2F, 0xAC, 0x70, 0x33, 0x64, 0x1B,
      0xEF, 0xE8, 0x99, 0x1F, 0x7A, 0x41, 0x2F, 0xAE, 0x66, 0x55, 0x22, 0x1D,
      0x36, 0x37, 0x2D, 0x7B, 0x6E, 0x7A, 0xE6, 0xFC, 0xEC, 0xC8, 0xC5, 0xC4,
      0x5D, 0xC6, 0x17, 0x4D, 0x76, 0x76, 0x6B, 0x41, 0x11, 0x52, 0x19, 0xAD,
      0xF0, 0xC1, 0xAE, 0xF4, 0x2D, 0x34, 0x08, 0x4E, 0x35, 0x4E, 0xF3, 0xB6,
      0x25, 0x59, 0xC1, 0xB9, 0xDA, 0x11, 0x75, 0xFA, 0xC8, 0x6A, 0xC3, 0x24,
      0x3A, 0x6C, 0xDF, 0x3A, 0xD6, 0xBE, 0xF5, 0x75, 0x70, 0x07, 0x82, 0xFB,
      0x9E, 0x44, 0xAF, 0xA3, 0x3B, 0x23, 0xCE, 0xEA, 0xC7, 0x51, 0x57, 0x25,
      0xD2, 0x8C, 0x93, 0x69, 0x26, 0xE8, 0x25, 0x62, 0xF4, 0x12, 0x21, 0x5A,
      0xF7, 0x12, 0x46, 0x8F, 0x5C, 0x64, 0x15, 0x87, 0x0F, 0xB1, 0xCF, 0x43,
      0xDB, 0x80, 0xE5, 0xE6, 0x69, 0x95, 0xAB, 0xF9, 0xC0, 0x03, 0x3E, 0x30,
      0xCA, 0x07, 0x7E, 0xE4, 0x03, 0x7D, 0x02, 0x80, 0xD6, 0xAB, 0x17, 0xCD,
      0x8E, 0xD9, 0x57, 0x96, 0xE7, 0x98, 0x43, 0xB4, 0x1C, 0x33, 0x6C, 0x52,
      0xD2, 0x38, 0x66, 0xD8, 0x30, 0x66, 0x54, 0x3B, 0x6E, 0x4A, 0x78, 0x68,
      0x1D, 0xDB, 0x83, 0xF2, 0x26, 0xE7, 0x39, 0x49, 0xF7, 0x13, 0x3F, 0x42,
      0x90, 0xEE, 0x2F, 0x95, 0xBA, 0xE3, 0xA5, 0x07, 0xCE, 0xC8, 0x7C, 0x93,
      0xFA, 0xE2, 0xFD, 0x64, 0xDE, 0xB8, 0x59, 0xF8, 0x60, 0x65, 0x3E, 0x45,
      0x93, 0x7E, 0x79, 0xAF, 0x10, 0x29, 0x1A, 0x27, 0xB2, 0x34, 0x4E, 0x1E,
      0x9B, 0x9B, 0x1F, 0xA1, 0x5D, 0xB9, 0xC3, 0xA8, 0x19, 0xB6, 0x83, 0xAF,
      0xF0, 0x52, 0x29, 0xCF, 0xCB, 0x3C, 0x3E, 0x0F, 0x04, 0xB5, 0x72, 0xA2,
      0x74, 0xCA, 0x77, 0xC3, 0x2E, 0x9A, 0xAA, 0x2B, 0xAF, 0x0C, 0xC4, 0x19,
      0x1C, 0x2E, 0xFA, 0x36, 0x3C, 0x0D, 0x96, 0xE1, 0x63, 0x57, 0x61, 0x05,
      0xE7, 0xA9, 0x29, 0x6F, 0x64, 0xED, 0xB3, 0xAF, 0x87, 0x6F, 0x26, 0xB8,
      0xEF, 0x4D, 0xF2, 0x8E, 0x9D, 0xAD, 0xAC, 0x23, 0x46, 0xEB, 0x0A, 0xD1,
      0x4B, 0xDB, 0x30, 0xCB, 0xC8, 0x45, 0xD6, 0xC8, 0x12, 0xA5, 0x72, 0x56,
      0xB9, 0x79, 0xFA, 0x27, 0x94, 0xCB, 0xFE, 0x78, 0xE5, 0x2F, 0x2A, 0x4E,
      0x2F, 0x26, 0xE7, 0x2C, 0xA9, 0x8A, 0xFD, 0xBA, 0x16, 0xBE, 0x86, 0xBB,
      0x66, 0xB7, 0x60, 0x41, 0x18, 0x6B, 0x19, 0xB2, 0xC6, 0x10, 0xF2, 0xD2,
      0x25, 0xE6, 0xEB, 0x96, 0xE5, 0x2E, 0x2D, 0x47, 0x2E, 0xA3, 0xBB, 0x5B,
      0x34, 0x9B, 0xEF, 0xE1, 0x2F, 0x08, 0xBB, 0xF0, 0x21, 0x32, 0x49, 0x27,
      0x9A, 0xA4, 0x97, 0x98, 0x82, 0xA0, 0xC5, 0x99, 0x80, 0x8D, 0x34, 0x5B,
      0x8F, 0xD6, 0xC5, 0x91, 0xF5, 0xFA, 0x28, 0xA5, 0xB2, 0xFB, 0xF7, 0x17,
      0xDD, 0xF7, 0x5E, 0xF0, 0xD8, 0x5F, 0xE6, 0xE9, 0x97, 0xE2, 0x9B, 0xB4,
      0x3B, 0xAA, 0x62, 0x39, 0x92, 0x66, 0x98, 0x48, 0x83, 0x41, 0xCA, 0x18,
      0xBD, 0x01, 0xCC, 0x32, 0x11, 0x46, 0xBF, 0xAD, 0xD0, 0x88, 0x52, 0xBC,
      0x01, 0x59, 0x12, 0xEE, 0x90, 0x8F, 0x81, 0x94, 0x2D, 0xD4, 0x94, 0xEF,
      0xF0, 0x81, 0x7E, 0xC1, 0x1C, 0x5A, 0xEF, 0xF2, 0x98, 0xE6, 0xA3, 0xF0,
      0xDF, 0x50, 0x36, 0x3E, 0xF7, 0x69, 0xE5, 0x09, 0xCF, 0xDF, 0x57, 0xB6,
      0x6C, 0xAE, 0xB4, 0x6A, 0xAE, 0xB0, 0x6A, 0x39, 0x65, 0xC7, 0x0B, 0x71,
      0xEA, 0xDE, 0x78, 0x48, 0xA4, 0x1B, 0xD5, 0xB0, 0x1C, 0x55, 0x63, 0x94,
      0xC4, 0x80, 0x59, 0x37, 0x56, 0x59, 0x37, 0x91, 0x6D, 0x9A, 0x72, 0xD7,
      0x73, 0x42, 0x9D, 0x2F, 0xDB, 0x7B, 0x09, 0xA1, 0x68, 0x85, 0x2A, 0x72,
      0xA4, 0x32, 0x37, 0x53, 0xE9, 0x9B, 0xE9, 0x18, 0x67, 0xE6, 0x91, 0x5D,
      0x6C, 0x27, 0xFF, 0xCB, 0x5F, 0x45, 0x9F, 0x5F, 0x19, 0x0F, 0x45, 0x74,
      0x58, 0x40, 0x0A, 0x2F, 0x20, 0xA5, 0x25, 0x20, 0xAD, 0xEA, 0x30, 0x08,
      0x86, 0x62, 0xDC, 0x15, 0x2F, 0x0C, 0xC3, 0x18, 0xEA, 0x87, 0x94, 0xB1,
      0x0E, 0xD7, 0xB1, 0x0E, 0x03, 0xD8, 0x4D, 0x5D, 0x38, 0x67, 0x6A, 0x09,
      0x3C, 0xB1, 0x0C, 0xE5, 0x58, 0x50, 0x64, 0x97, 0x4D, 0xB2, 0x48, 0xAF,
      0x5A, 0x2D, 0xD0, 0x18, 0x13, 0x68, 0x3C, 0x10, 0x68, 0xDE, 0xED, 0x9D,
      0x2F, 0xEC, 0x5D, 0x42, 0xEF, 0x33, 0x3D, 0xDA, 0x12, 0x07, 0x2F, 0xCB,
      0xDF, 0x7C, 0x0A, 0x52, 0x36, 0x66, 0x8F, 0x19, 0x37, 0x29, 0xB9, 0x38,
      0x0E, 0x3B, 0x37, 0x6E, 0x46, 0x7C, 0x64, 0xAB, 0x52, 0x76, 0x9E, 0xA5,
      0xEC, 0xFE, 0x51, 0xD9, 0x2F, 0xA9, 0x2F, 0x61, 0xB6, 0xB2, 0xCF, 0x2C,
      0xE5, 0xE0, 0xA1, 0xEE, 0xE0, 0xA1, 0xCE, 0xE0, 0x21, 0x66, 0xC8, 0xF0,
      0x89, 0xC8, 0x5B, 0x9E, 0xF1, 0x23, 0x46, 0x49, 0x6C, 0x58, 0x72, 0xAD,
      0x49, 0x32, 0xC3, 0x04, 0x21, 0xE9, 0x41, 0x48, 0x3A, 0x11, 0x12, 0x66,
      0xE8, 0x8D, 0x93, 0x51, 0x3F, 0x78, 0x26, 0x8C, 0x18, 0xFF, 0x37, 0x8A,
      0x10, 0x09, 0x22, 0x44, 0xDD, 0x11, 0x57, 0xEA, 0xA2, 0xC4, 0xB9, 0x31,
      0x83, 0x5E, 0xC9, 0x83, 0x26, 0x29, 0x8D, 0x26, 0x29, 0x4C, 0x93, 0x14,
      0x86, 0x49, 0x1A, 0xEB, 0x6A, 0x1A, 0x4B, 0x94, 0xD6, 0xD0, 0x9C, 0xDE,
      0x88, 0xCD, 0xE4, 0x86, 0xE4, 0x74, 0x5A, 0x82, 0x75, 0xE6, 0xE9, 0x8C,
      0xD5, 0xA9, 0x74, 0x53, 0x72, 0xE2, 0x6D, 0x72, 0xE2, 0x08, 0x39, 0x51,
      0x48, 0x49, 0xA9, 0xAF, 0x04, 0x41, 0x3A, 0x76, 0x3B, 0x8E, 0x68, 0x9F,
      0x53, 0x61, 0x99, 0x51, 0x65, 0x26, 0x34, 0x7F, 0x2C, 0x34, 0x7F, 0x24,
      0x34, 0xBF, 0x27, 0xFC, 0x4E, 0x2C, 0xB4, 0x65, 0x8A, 0x5C, 0x51, 0xBC,
      0x64, 0x0F, 0x52, 0xC1, 0x96, 0xDC, 0x32, 0xAB, 0x87, 0x6B, 0x5A, 0x3E,
      0xC2, 0x7E, 0x68, 0x7E, 0xFE, 0xE1, 0xDA, 0xB3, 0x8F, 0x37, 0xC4, 0xF1,
      0x13, 0x7C, 0x28, 0xF9, 0xCE, 0x52, 0x0F, 0xA2, 0x1A, 0x04, 0xE9, 0x01,
      0xFC, 0xC4, 0xC1, 0x82, 0x49, 0xFF, 0x64, 0x85, 0xB2, 0x42, 0x53, 0x1D,
      0xAC, 0xCC, 0xB7, 0x68, 0xD2, 0x5F, 0xA1, 0x4C, 0x92, 0x8A, 0x49, 0x52,
      0x11, 0x49, 0x7A, 0x99, 0x24, 0xAD, 0x25, 0x4F, 0x66, 0xD1, 0xDE, 0x6D,
      0xC7, 0x76, 0x6C, 0x3C, 0x59, 0xBF, 0x36, 0xA3, 0xDA, 0x8C, 0xF4, 0x52,
      0x4C, 0x7A, 0x79, 0x85, 0xF4, 0xF2, 0x72, 0x85, 0x32, 0xA2, 0xAB, 0x45,
      0xE4, 0x67, 0x57, 0xC9, 0xCF, 0xC4, 0xE4, 0xE7, 0x3D, 0xE4, 0xE7, 0x75,
      0x95, 0xD2, 0x6C, 0xC6, 0x5B, 0x57, 0x5C, 0xBB, 0xFD, 0xC9, 0x7A, 0x4B,
      0x28, 0x62, 0xDC, 0xBB, 0xC5, 0xB8, 0x37, 0xC2, 0xB8, 0x2F, 0xAE, 0x1E,
      0x6D, 0xAC, 0x19, 0xCF, 0xAD, 0x7B, 0xB1, 0x9B, 0xC8, 0x71, 0xCC, 0xAD,
      0xB5, 0x3A, 0xC6, 0x58, 0xC3, 0x69, 0x93, 0x72, 0xDA, 0x5E, 0x70, 0xDA,
      0x7E, 0xE2, 0xB4, 0x77, 0x73, 0x3B, 0x09, 0xAD, 0x7D, 0xFE, 0xD5, 0x4C,
      0xD7, 0x42, 0xEA, 0xFA, 0x6C, 0xAA, 0x85, 0x04, 0x25, 0x93, 0xA0, 0x26,
      0x24, 0xA8, 0x27, 0x92, 0xF8, 0x9B, 0x92, 0xA4, 0xA6, 0x21, 0x10, 0xEC,
      0xC9, 0xF7, 0xA6, 0x61, 0xB7, 0xE5, 0x97, 0xDB, 0x3E, 0x71, 0xE8, 0x51,
      0xD2, 0xFD, 0x64, 0x53, 0xD7, 0x53, 0x47, 0xCE, 0xD3, 0x2D, 0xD4, 0x67,
      0xAE, 0xA8, 0xFE, 0x34, 0xBF, 0xAA, 0x82, 0xAD, 0x13, 0x07, 0x89, 0x33,
      0x1C, 0x22, 0x4C, 0x1C, 0xC2, 0xCB, 0x7C, 0x0A, 0xA6, 0x0E, 0x27, 0xF7,
      0x27, 0xF9, 0xCB, 0x7C, 0x71, 0xEA, 0x4C, 0xFA, 0x62, 0x27, 0xFD, 0x8A,
      0x26, 0x03, 0xF2, 0x5E, 0x85, 0xA4, 0x70, 0x06, 0x28, 0x9C, 0x01, 0xB2,
      0x92, 0x72, 0xEE, 0x00, 0xAE, 0xF5, 0x6A, 0x66, 0x97, 0xC4, 0xAB, 0x92,
      0xED, 0x92, 0x57, 0x6B, 0xF3, 0xA9, 0x48, 0x4C, 0x51, 0x42, 0xE6, 0x8A,
      0xCB, 0xB9, 0x62, 0x28, 0x02, 0xBB, 0x06, 0xBD, 0x2A, 0x9B, 0xB6, 0x42,
      0x51, 0x6B, 0x3F, 0x45, 0x09, 0xB9, 0x55, 0x48, 0xBA, 0x24, 0xC4, 0xB7,
      0x8B, 0xB2, 0xBA, 0xAF, 0x7A, 0x53, 0x1B, 0xB7, 0xE5, 0x33, 0x6D, 0xBA,
      0x3B, 0xAA, 0xBA, 0x3B, 0x2A, 0x95, 0x90, 0x7B, 0x3A, 0x4B, 0xF9, 0x5D,
      0x27, 0x84, 0x82, 0x80, 0x9A, 0xF3, 0x6E, 0x05, 0xD5, 0x76, 0xC3, 0x8C,
      0xEA, 0x0F, 0x54, 0xD3, 0x87, 0xAB, 0x2B, 0x6E, 0x32, 0x0B, 0x6E, 0xB1,
      0x22, 0x9A, 0x29, 0x70, 0x3C, 0xC5, 0x5E, 0x86, 0x94, 0x2B, 0x99, 0x96,
      0x21, 0xA7, 0x64, 0xA8, 0xBB, 0xB2, 0x44, 0xAE, 0x0C, 0x04, 0x07, 0x73,
      0x83, 0x99, 0xC5, 0x6E, 0x53, 0x41, 0x65, 0x6A, 0x10, 0x5F, 0x05, 0x97,
      0xBE, 0x42, 0x60, 0xDE, 0x44, 0xA6, 0x8A, 0xD3, 0x03, 0x27, 0x03, 0x70,
      0xEA, 0x4C, 0x05, 0x60, 0xA7, 0x02, 0x8B, 0xA6, 0x82, 0xF2, 0x5F, 0x87,
      0xA5, 0xF2, 0x3B, 0x4A, 0x95, 0x94, 0x28, 0xC1, 0x09, 0x3A, 0xD0, 0x7D,
      0x1D, 0x99, 0x03, 0x5D, 0x87, 0xE9, 0x0D, 0xDB, 0xF9, 0xED, 0xA5, 0x0A,
      0x3E, 0x11, 0xB5, 0x97, 0x28, 0x99, 0x89, 0x18, 0x0D, 0xDB, 0x05, 0x6D,
      0xA5, 0x4A, 0x4A, 0x94, 0xE0, 0x7A, 0xDB, 0xD0, 0xFD, 0xED, 0xE0, 0x40,
      0x67, 0x10, 0x83, 0xE5, 0xDA, 0xC7, 0x2B, 0xFD, 0x48, 0x49, 0x3F, 0x0F,
      0xD7, 0xCF, 0xC3, 0x88, 0x5A, 0xB3, 0xAE, 0xB7, 0x05, 0x43, 0xD6, 0xD7,
      0x58, 0xA5, 0x6A, 0xE0, 0xAF, 0xB3, 0x0A, 0x07, 0x1B, 0x8E, 0xDF, 0x6C,
      0x0A, 0xAB, 0xAF, 0xDD, 0x75, 0xFF, 0x2C, 0x41, 0x8D, 0xD2, 0xFB, 0x67,
      0xB1, 0xA3, 0xA4, 0xD3, 0xE3, 0x94, 0x58, 0x1E, 0xC9, 0x63, 0x3A, 0xB9,
      0x56, 0x0D, 0xE6, 0x74, 0x4A, 0xF5, 0x74, 0x2A, 0x65, 0x1A, 0x04, 0x6F,
      0x9C, 0x0A, 0x7D, 0x1D, 0x8E, 0x57, 0x03, 0xA7, 0x20, 0xA2, 0xF8, 0x4D,
      0xE4, 0x99, 0xB7, 0x31, 0x69, 0xFD, 0x5C, 0x9C, 0x1A, 0x58, 0x21, 0xB7,
      0x58, 0xC8, 0x2D, 0xB8, 0xC2, 0x03, 0x07, 0x5B, 0x11, 0xFF, 0x5F, 0x24,
      0xE4, 0xE2, 0xD4, 0x50, 0x44, 0x22, 0x28, 0xE2, 0x82, 0x83, 0x3C, 0x84,
      0x90, 0x83, 0x53, 0x03, 0x2B, 0xE2, 0x14, 0x8B, 0x38, 0x05, 0x62, 0x0E,
      0x28, 0xE1, 0x85, 0x88, 0x9B, 0x70, 0x6A, 0x60, 0xC5, 0xEC, 0xE2, 0x01,
      0x76, 0xC1, 0x35, 0x76, 0xD6, 0x8D, 0x96, 0xD0, 0xA1, 0x3A, 0xDC, 0x6C,
      0x8A, 0x6F, 0xD4, 0xA1, 0x87, 0xEB, 0x8F, 0xDF, 0xBE, 0x10, 0x39, 0x46,
      0xC0, 0xAB, 0x81, 0x1B, 0x23, 0x60, 0xC7, 0x88, 0x85, 0xE3, 0x65, 0xB9,
      0x8F, 0x49, 0xC8, 0xF7, 0xE9, 0x25, 0x6A, 0xE0, 0x95, 0x60, 0xDF, 0x67,
      0xA0, 0xE5, 0xD0, 0x77, 0xC8, 0xE7, 0x4F, 0xD1, 0xCF, 0xFE, 0x7F, 0x66,
      0xFF, 0x68, 0x17, 0x67, 0xE5, 0x7A, 0x56, 0x53, 0x53, 0xB5, 0xA8, 0xFD,
      0xC5, 0x6A, 0x15, 0x88, 0x0D, 0x42, 0x06, 0xA9, 0xAF, 0x5D, 0x7F, 0xEF,
      0xF8, 0x3F, 0x0B, 0x10, 0x3B, 0xD9
    };
  };

  bool         write_image_to_disk          (const wchar_t* image_path, unsigned int width, unsigned int height, const void*  pixels,          int quantization_bits, format fmt);//, reshade::api::display* display);
  bool         write_hdr_chunks             (const wchar_t* image_path, unsigned int width, unsigned int height, const float* luminance_array, int quantization_bits);//,                                 reshade::api::display* display);
  cLLi_Payload calculate_content_light_info (const float*   luminance,  unsigned int width, unsigned int height);
  bool         copy_to_clipboard            (const wchar_t* image_path);
  bool         remove_chunk                 (const char*    chunk_name, void* data, size_t& size);
  uint32_t     crc32                        (const void*    typeless_data, size_t offset, size_t len, uint32_t crc);

  struct ParamsPQ
  {
    DirectX::XMVECTOR N,       M;
    DirectX::XMVECTOR C1, C2, C3;
    DirectX::XMVECTOR MaxPQ;
    DirectX::XMVECTOR RcpN, RcpM;
  };

  static const ParamsPQ PQ =
  {
    DirectX::XMVectorReplicate  (2610.0 / 4096.0 / 4.0),   // N
    DirectX::XMVectorReplicate  (2523.0 / 4096.0 * 128.0), // M
    DirectX::XMVectorReplicate  (3424.0 / 4096.0),         // C1
    DirectX::XMVectorReplicate  (2413.0 / 4096.0 * 32.0),  // C2
    DirectX::XMVectorReplicate  (2392.0 / 4096.0 * 32.0),  // C3
    DirectX::XMVectorReplicate  (125.0f),                  // MaxPQ
    DirectX::XMVectorReciprocal (DirectX::XMVectorReplicate (2610.0 / 4096.0 / 4.0)),
    DirectX::XMVectorReciprocal (DirectX::XMVectorReplicate (2523.0 / 4096.0 * 128.0)),
  };

  constexpr DirectX::XMMATRIX c_from709to2020 =
  {
    { 0.627403914928436279296875f,     0.069097287952899932861328125f,    0.01639143936336040496826171875f, 0.0f },
    { 0.3292830288410186767578125f,    0.9195404052734375f,               0.08801330626010894775390625f,    0.0f },
    { 0.0433130674064159393310546875f, 0.011362315155565738677978515625f, 0.895595252513885498046875f,      0.0f },
    { 0.0f,                            0.0f,                              0.0f,                             1.0f }
  };

  constexpr DirectX::XMMATRIX c_from709toXYZ =
  {
    { 0.4123907983303070068359375f,  0.2126390039920806884765625f,   0.0193308182060718536376953125f, 0.0f },
    { 0.3575843274593353271484375f,  0.715168654918670654296875f,    0.119194783270359039306640625f,  0.0f },
    { 0.18048079311847686767578125f, 0.072192318737506866455078125f, 0.950532138347625732421875f,     0.0f },
    { 0.0f,                          0.0f,                           0.0f,                            1.0f }
  };

  constexpr DirectX::XMMATRIX c_from2020toXYZ =
  {
    { 0.636958062648773193359375f,  0.26270020008087158203125f,      0.0f,                           0.0f },
    { 0.144616901874542236328125f,  0.677998065948486328125f,        0.028072692453861236572265625f, 0.0f },
    { 0.1688809692859649658203125f, 0.0593017153441905975341796875f, 1.060985088348388671875f,       0.0f },
    { 0.0f,                         0.0f,                            0.0f,                           1.0f }
  };

  static auto LinearToPQ = [](DirectX::XMVECTOR N)
  {
    using namespace DirectX;

    XMVECTOR ret =
      XMVectorPow (XMVectorDivide (XMVectorMax (N, g_XMZero), PQ.MaxPQ), PQ.N);

    XMVECTOR nd =
      XMVectorDivide (
        XMVectorMultiplyAdd (PQ.C2, ret, PQ.C1),
        XMVectorMultiplyAdd (PQ.C3, ret, g_XMOne)
      );

    return
      XMVectorPow (nd, PQ.M);
  };

  static auto PQToLinear = [](DirectX::XMVECTOR N)
  {
    using namespace DirectX;

    XMVECTOR ret =
      XMVectorPow (XMVectorMax (N, g_XMZero), PQ.RcpM);

    XMVECTOR nd =
      XMVectorDivide (
        XMVectorMax (XMVectorSubtract (ret, PQ.C1), g_XMZero),
                     XMVectorSubtract (     PQ.C2,
                          XMVectorMultiply (PQ.C3, ret)));

    ret =
      XMVectorMultiply (
        XMVectorPow (nd, PQ.RcpN), PQ.MaxPQ
      );

    return ret;
  };
}

sk_hdr_png::cLLi_Payload
sk_hdr_png::calculate_content_light_info (const float* luminance, unsigned int width, unsigned int height)
{
  using namespace DirectX;
  using namespace DirectX::PackedVector;

  cLLi_Payload clli = { };

  if (luminance == nullptr || width == 0 || height == 0)
    return clli;

  float N         =       0.0f;
  float fLumAccum =       0.0f;
  float fMaxLum   =       0.0f;
  float fMinLum   = 5240320.0f;

  float fScanlineLum = 0.0f;

  const float* pixel_luminance = luminance;

  for (size_t y = 0; y < height; y++)
  {
    fScanlineLum = 0.0f;

    for (size_t x = 0; x < width ; x++)
    {
      fMaxLum = std::max (fMaxLum, *pixel_luminance);
      fMinLum = std::min (fMinLum, *pixel_luminance);

      fScanlineLum += *pixel_luminance++;
    }

    fLumAccum +=
      (fScanlineLum / static_cast <float> (width));
    ++N;
  }

  if (N > 0.0)
  {
    pixel_luminance = luminance;

    // 0 nits - 10k nits (appropriate for screencap, but not HDR photography)
    fMinLum = std::clamp (fMinLum, 0.0f,    125.0f);
    fMaxLum = std::clamp (fMaxLum, fMinLum, 125.0f);

    const float fLumRange =
            (fMaxLum - fMinLum);

    auto        luminance_freq = std::make_unique <uint32_t []> (65536);
    ZeroMemory (luminance_freq.get (),     sizeof (uint32_t)  *  65536);

    for (size_t y = 0; y < height * width; y++)
    {
      luminance_freq [
        std::clamp ( (int)
          std::roundf (
            (*pixel_luminance++ - fMinLum)     /
                                    (fLumRange / 65536.0f) ),
                                              0, 65535 ) ]++;
    }

          double percent  = 100.0;
    const double img_size = (double)width *
                            (double)height;

    // Now that we have the frequency distribution, let's claim our prize...
    //
    //   * Calculate the 99.5th percentile luminance and use it as MaxCLL
    //
    for (auto i = 65535; i >= 0; --i)
    {
      percent -=
        100.0 * ((double)luminance_freq [i] / img_size);

      if (percent <= 99.5)
      {
        fMaxLum = fMinLum + (fLumRange * ((float)i / 65536.0f));
        break;
      }
    }

    SetUint32 (clli.max_cll,
      static_cast <uint32_t> ((80.0f *  fMaxLum       ) / 0.0001f));
    SetUint32 (clli.max_fall,
      static_cast <uint32_t> ((80.0f * (fLumAccum / N)) / 0.0001f));
  }

  return clli;
}

uint32_t
sk_hdr_png::crc32 (const void* typeless_data, size_t offset, size_t len, uint32_t crc)
{
	auto data = reinterpret_cast<const BYTE *>(typeless_data);

	if (data == nullptr || len == 0)
	{
		return static_cast<uint32_t>(-1);
	}

	uint32_t c;

	static uint32_t
	    png_crc_table[256] = { };
	if (png_crc_table[ 0 ] == 0)
	{
		for (auto i = 0 ; i < 256 ; ++i)
		{
			c = i;

			for (auto j = 0 ; j < 8 ; ++j)
			{
				if ((c & 1) == 1) c = (0xEDB88320 ^ ((c >> 1) & 0x7FFFFFFF));
				else              c = (              (c >> 1) & 0x7FFFFFFF);
			}

			png_crc_table [i] = c;
		}
	}

	c = (crc ^ 0xffffffff);

	for (auto k = offset ; k < (offset + len) ; ++k)
	{
		c = png_crc_table [(c ^ data [k]) & 255] ^
		                  ((c >> 8)       & 0xFFFFFF);
	}

	return (c ^ 0xffffffff);
}

//
// To convert an image passed to an encoder that does not understand HDR,
//   but that we actually fed HDR pixels to... perform the following:
//
//  1. Remove gAMA chunk  (Prevents SKIV from recognizing as HDR)
//  2. Remove sRGB chunk  (Prevents Discord from rendering in HDR)
//
//  3. Add cICP  (The primary way of defining HDR10) 
//  4. Add iCCP  (Required for Discord to render in HDR)
//
//  (5) Add cLLi  [Unnecessary, but probably a good idea]
//  (6) Add cHRM  [Unnecessary, but probably a good idea]
//
bool
sk_hdr_png::remove_chunk (const char* chunk_name, void* data, size_t& size)
{
	if (chunk_name == nullptr || data == nullptr || size < 12 || strlen(chunk_name) < 4)
	{
		return false;
	}

	size_t   erase_pos = 0;
	uint8_t* erase_ptr = nullptr;

	// Effectively a string search, but ignoring nul-bytes in both
	//   the character array being searched and the pattern...
	std::string_view data_view((const char*)data, size);
	if (erase_pos  = data_view.find(chunk_name, 0, 4);
	    erase_pos == data_view.npos)
	{
		return false;
	}

	erase_pos -= 4; // Rollback to the chunk's length field
	erase_ptr = ((uint8_t*)data + erase_pos);

	uint32_t chunk_size = *(uint32_t*)erase_ptr;

	// Length is Big Endian, Intel/AMD CPUs are Little Endian
#if (defined _M_IX86) || (defined _M_X64)
		chunk_size = _byteswap_ulong(chunk_size);
#endif

	size_t size_to_erase = (size_t)12 + chunk_size;

	memmove(erase_ptr,
	        erase_ptr             + size_to_erase,
	             size - erase_pos - size_to_erase);

	size -= size_to_erase;

	return true;
}

bool
sk_hdr_png::write_hdr_chunks (const wchar_t* image_path, unsigned int width, unsigned int height, const float* luminance, int quantization_bits)//, reshade::api::display* display)
{
	if (image_path == nullptr || width == 0 || height == 0 || quantization_bits < 6)
	{
		return false;
	}

	// 16-byte alignment is mandatory for SIMD processing
	if ((reinterpret_cast<uintptr_t>(luminance) & 0xF) != 0)
	{
		return false;
	}

	FILE*
	    fPNG = _wfopen(image_path, L"r+b");
	if (fPNG != nullptr)
	{
		fseek(fPNG, 0, SEEK_END);
		size_t size = ftell(fPNG);
		rewind(fPNG);

		auto data = std::make_unique<uint8_t[]>(size);

		if (! data)
		{
			fclose(fPNG);
			return false;
		}

		fread(data.get(), size, 1, fPNG);
		rewind(                    fPNG);

		remove_chunk("sRGB", data.get(), size);
		remove_chunk("gAMA", data.get(), size);

		fwrite(data.get(), size, 1, fPNG);

		// Truncate the file
		_chsize(_fileno(fPNG), static_cast<long>(size));

		size_t         insert_pos = 0;
		const uint8_t* insert_ptr = nullptr;

		// Effectively a string search, but ignoring nul-bytes in both
		//   the character array being searched and the pattern...
		std::string_view  data_view((const char *)data.get(), size);
		if (insert_pos  = data_view.find("IDAT", 0, 4);
		    insert_pos == data_view.npos)
		{
			fclose(fPNG);
			return false;
		}

		insert_pos -= 4; // Rollback to the chunk's length field
		insert_ptr = (data.get() + insert_pos);

		fseek(fPNG, static_cast<long>(insert_pos), SEEK_SET);

		struct Chunk {
			uint32_t      len;
			unsigned char name [4];
			void*         data;
			uint32_t      crc;
			uint32_t      _native_len;

			void write (FILE* fStream)
			{
				// Length is Big Endian, Intel/AMD CPUs are Little Endian
				if (_native_len == 0)
				{
					_native_len = len;
#if (defined _M_IX86) || (defined _M_X64)
					len         = _byteswap_ulong(_native_len);
#endif
				}

				crc = crc32(data, 0, _native_len, crc32(name, 0, 4, 0x0));

#if (defined _M_IX86) || (defined _M_X64)
				crc = _byteswap_ulong(crc);
#endif

				fwrite(&len, 8,           1, fStream);
				fwrite(data, _native_len, 1, fStream);
				fwrite(&crc, 4,           1, fStream);
			};
		};

		uint8_t cicp_data[] = {
			9,  // BT.2020 Color Primaries
			16, // ST.2084 EOTF (PQ)
			0,  // Identity Coefficients
			1,  // Full Range
		};

		// Embedded ICC Profile so that Discord will render in HDR
		iCCP_Payload iccp_data;

		cHRM_Payload chrm_data; // Rec 2020 chromaticity
		sBIT_Payload sbit_data; // Bits in original source (max=16)
		mDCv_Payload mdcv_data; // Display capabilities
		cLLi_Payload clli_data; // Content light info

		clli_data = calculate_content_light_info(luminance, width, height);

		unsigned char sBIT_quantized = static_cast<unsigned char>(quantization_bits);
		sbit_data = { sBIT_quantized, sBIT_quantized, sBIT_quantized };

		Chunk iccp_chunk = {sizeof(iCCP_Payload), {'i','C','C','P'}, &iccp_data};
		Chunk cicp_chunk = {sizeof(cicp_data),    {'c','I','C','P'}, &cicp_data};
		Chunk clli_chunk = {sizeof(clli_data),    {'c','L','L','i'}, &clli_data};
		Chunk sbit_chunk = {sizeof(sbit_data),    {'s','B','I','T'}, &sbit_data};
		Chunk chrm_chunk = {sizeof(chrm_data),    {'c','H','R','M'}, &chrm_data};

		iccp_chunk.write(fPNG);
		cicp_chunk.write(fPNG);
		clli_chunk.write(fPNG);
		sbit_chunk.write(fPNG);
		chrm_chunk.write(fPNG);

#if 0
		///
		/// Mastering metadata can be added, provided you are able to read this info
		/// from the user's EDID.
		///
		if (display != nullptr)
		{
			auto colorimetry = display->get_colorimetry();
			auto luminance_caps = display->get_luminance_caps();

			sk_hdr_png::SetUint32 (mdcv_data.luminance.minimum,
			  static_cast <uint32_t> (round (luminance_caps.min_nits / 0.0001f)));
			sk_hdr_png::SetUint32 (mdcv_data.luminance.maximum,
			  static_cast <uint32_t> (round (luminance_caps.max_nits / 0.0001f)));

			sk_hdr_png::SetUint32 (mdcv_data.primaries.red_x,
			  static_cast <uint32_t> (round (colorimetry.red [0] / 0.00002)));
			sk_hdr_png::SetUint32 (mdcv_data.primaries.red_y,
			  static_cast <uint32_t> (round (colorimetry.red [1] / 0.00002)));

			sk_hdr_png::SetUint32 (mdcv_data.primaries.green_x,
			  static_cast <uint32_t> (round (colorimetry.green [0] / 0.00002)));
			sk_hdr_png::SetUint32 (mdcv_data.primaries.green_y,
			  static_cast <uint32_t> (round (colorimetry.green [1] / 0.00002)));

			sk_hdr_png::SetUint32 (mdcv_data.primaries.blue_x,
			  static_cast <uint32_t> (round (colorimetry.blue [0] / 0.00002)));
			sk_hdr_png::SetUint32 (mdcv_data.primaries.blue_y,
			  static_cast <uint32_t> (round (colorimetry.blue [1] / 0.00002)));

			sk_hdr_png::SetUint32 (mdcv_data.white_point.x,
			  static_cast <uint32_t> (round (colorimetry.white [0] / 0.00002)));
			sk_hdr_png::SetUint32 (mdcv_data.white_point.y,
			  static_cast <uint32_t> (round (colorimetry.white [1] / 0.00002)));

			Chunk mdcv_chunk = {sizeof(mdcv_data),    {'m','D','C','v'}, &mdcv_data};
			mdcv_chunk.write(fPNG);
		}
#endif

		// Write the remainder of the original file
		fwrite(insert_ptr, size - insert_pos, 1, fPNG);
		fflush(fPNG);
		fclose(fPNG);

		return true;
	}

	return false;
}

bool
sk_hdr_png::write_image_to_disk (const wchar_t* image_path, unsigned int width, unsigned int height, const void* pixels, int quantization_bits, format fmt)//, reshade::api::display* display)
{
	using namespace DirectX;
	using namespace DirectX::PackedVector;

	// PNG only supports 8-bpc and 16-bpc pixels; the bpc refers to the size of the pixel during encode/decode.
	//
	//   * We have a 3-channel RGB image, thus 48-bpp when decoded.
	//
	// Space savings are possible by quantizing to alternate bit depths before encoding, 10-bpc is a sane minimum for HDR.
	WICPixelFormatGUID wic_format = GUID_WICPixelFormat48bppRGB;

	if (image_path == nullptr || width == 0 || height == 0 || (fmt != format::r16g16b16a16_float && fmt != format::r10g10b10a2_unorm && fmt != format::b10g10r10a2_unorm))
	{
		return false;
	}

	// 16-byte alignment is mandatory for SIMD processing
	if (pixels == nullptr || (reinterpret_cast<uintptr_t>(pixels) & 0xF) != 0)
	{
		return false;
	}

	if (quantization_bits < 6 || quantization_bits > 16)
	{
		return false;
	}

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		return false;
	}

	const bool unitialize_com = hr == S_OK; // S_FALSE if it was already initialized

	com_ptr<IWICImagingFactory> factory;
	com_ptr<IWICBitmapEncoder> encoder;
	com_ptr<IWICBitmapFrameEncode> bitmap_frame;
	com_ptr<IPropertyBag2> property_bag;
	com_ptr<IWICStream> stream;

	if (SUCCEEDED(hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast<LPVOID *>(&factory))) &&
		SUCCEEDED(hr = factory->CreateStream(&stream)) &&
		SUCCEEDED(hr = stream->InitializeFromFilename(image_path, GENERIC_WRITE)) &&
		SUCCEEDED(hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
		SUCCEEDED(hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache)) &&
		SUCCEEDED(hr = encoder->CreateNewFrame(&bitmap_frame, &property_bag)) &&
		SUCCEEDED(hr = bitmap_frame->Initialize(property_bag.get())) &&
		SUCCEEDED(hr = bitmap_frame->SetSize(width, height)) &&
		SUCCEEDED(hr = bitmap_frame->SetPixelFormat(&wic_format)) &&
		IsEqualGUID(wic_format, GUID_WICPixelFormat48bppRGB))
	{
		UINT row_stride = (width * 48 + 7) / 8;
		UINT buffer_size = height * row_stride;

		hr = E_OUTOFMEMORY;
		const auto png_buffer = static_cast<BYTE *>(_aligned_malloc(sizeof(BYTE) * buffer_size, 16));
		const auto rgba32_scanline = static_cast<XMFLOAT4 *>(_aligned_malloc(sizeof(XMFLOAT4) * width, 16));
		const auto luminance = static_cast<float *>(_aligned_malloc(sizeof(float) * width * height, 16));

		if (png_buffer != nullptr && rgba32_scanline != nullptr && luminance != nullptr)
		{
			auto QUANTIZE_FP32_TO_UNORM16 = [](XMVECTOR& rgb32,int bit_reduce,uint16_t*& output)
			{
				const int quant_postscale = 1UL << bit_reduce;
				const float quant_prescale = static_cast<float>(quant_postscale);

				*(output++) = static_cast<uint16_t>(std::min (65535, static_cast<int>(std::roundf ((XMVectorGetX (rgb32) * quant_prescale)) * 65536.0f) / quant_postscale));
				*(output++) = static_cast<uint16_t>(std::min (65535, static_cast<int>(std::roundf ((XMVectorGetY (rgb32) * quant_prescale)) * 65536.0f) / quant_postscale));
				*(output++) = static_cast<uint16_t>(std::min (65535, static_cast<int>(std::roundf ((XMVectorGetZ (rgb32) * quant_prescale)) * 65536.0f) / quant_postscale));
			};

			if (fmt == format::r10g10b10a2_unorm || fmt == format::b10g10r10a2_unorm)
			{
				uint16_t* png_pixels = (uint16_t *)png_buffer;
				uint32_t* src_pixels = (uint32_t *)pixels;

				auto pixel_luminance = luminance;

				for (size_t i = 0; i < width * height; i++)
				{
					const uint32_t color = *reinterpret_cast<const uint32_t *>(src_pixels++);

					// Multiply by 64 and +/- 1 to get 10-bit range (0-1023) into 16-bit range (0-65535)
					const uint16_t c[] = { (((( color & 0x000003FFU)         + 1U) * 64U) & 0xFFFFU) - 1U,
					                       (((((color & 0x000FFC00U) >> 10U) + 1U) * 64U) & 0xFFFFU) - 1U,
					                       (((((color & 0x3FF00000U) >> 20U) + 1U) * 64U) & 0xFFFFU) - 1U };

					const int r = fmt == format::b10g10r10a2_unorm ? 2 : 0;
					const int g = 1;
					const int b = fmt == format::b10g10r10a2_unorm ? 0 : 2;

					XMVECTOR rgb =
						XMVectorSet (static_cast<float>(c [r]) / 65535.0f,
						             static_cast<float>(c [g]) / 65535.0f,
						             static_cast<float>(c [b]) / 65535.0f, 1.0f);

					if (quantization_bits < 10) {
						QUANTIZE_FP32_TO_UNORM16 (rgb, quantization_bits, png_pixels);
					} else {
						quantization_bits = 10; // Cap to 10-bpc
						*(png_pixels++) = c [r];
						*(png_pixels++) = c [g];
						*(png_pixels++) = c [b];
					}

					*pixel_luminance++ =
						XMVectorGetY (
						  XMVector3Transform (
						    PQToLinear (XMVectorSaturate (rgb)), c_from2020toXYZ
						  )
						);
				}
			}
			else if (fmt == format::r16g16b16a16_float)
			{
				uint16_t* png_pixels = (uint16_t *)png_buffer;
				uint16_t* src_pixels = (uint16_t *)pixels;

				auto pixel_luminance = luminance;

				for (size_t y = 0; y < height; y++)
				{
					XMFLOAT4* rgba32_pixels = rgba32_scanline;

					XMConvertHalfToFloatStream (
					  (float *)rgba32_pixels, sizeof (float),
					           src_pixels,    sizeof (HALF), 4 * width
					);

					for (size_t x = 0; x < width ; x++)
					{
						XMVECTOR rgb =
							XMLoadFloat4 (rgba32_pixels++);

						*pixel_luminance++ =
							XMVectorGetY (
								XMVector3Transform (rgb, c_from709toXYZ)
							);

						rgb =
							LinearToPQ (
								XMVectorMax (
									XMVector3Transform (rgb, c_from709to2020),
									g_XMZero )
							);

						if (quantization_bits < 16)
							QUANTIZE_FP32_TO_UNORM16 (rgb, quantization_bits, png_pixels);
						else
						{
							*(png_pixels++) = static_cast<uint16_t>(std::min (65535, static_cast<int>(XMVectorGetX (rgb) * 65536.0f)));
							*(png_pixels++) = static_cast<uint16_t>(std::min (65535, static_cast<int>(XMVectorGetY (rgb) * 65536.0f)));
							*(png_pixels++) = static_cast<uint16_t>(std::min (65535, static_cast<int>(XMVectorGetZ (rgb) * 65536.0f)));
						}

						src_pixels += 4;
					}
				}
			}

			if (SUCCEEDED(hr = bitmap_frame->WritePixels(height, row_stride, buffer_size, png_buffer)) &&
				SUCCEEDED(hr = bitmap_frame->Commit()) &&
				SUCCEEDED(hr = encoder->Commit()))
			{
				hr = write_hdr_chunks(image_path, width, height, luminance, quantization_bits) ? S_OK : E_FAIL;
			}
		}

		_aligned_free(png_buffer);
		_aligned_free(rgba32_scanline);
		_aligned_free(luminance);
	}

	if (unitialize_com)
	{
		CoUninitialize();
	}

	return SUCCEEDED(hr);
}

#pragma pop_macro("_XM_F16C_INTRINSICS_")

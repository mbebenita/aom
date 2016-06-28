/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_COMMON_ENUMS_H_
#define AV1_COMMON_ENUMS_H_

#include "./aom_config.h"
#include "aom/aom_integer.h"

#ifdef __cplusplus
extern "C" {
#endif
#define MAX_SB_SIZE_LOG2 6
#define MAX_SB_SIZE (1 << MAX_SB_SIZE_LOG2)
#define MAX_SB_SQUARE (MAX_SB_SIZE * MAX_SB_SIZE)

#define MI_SIZE_LOG2 3
#define MI_BLOCK_SIZE_LOG2 (MAX_SB_SIZE_LOG2 - MI_SIZE_LOG2)  // 64 = 2^6

#define MI_SIZE (1 << MI_SIZE_LOG2)              // pixels per mi-unit
#define MI_BLOCK_SIZE (1 << MI_BLOCK_SIZE_LOG2)  // mi-units per max block

#define MI_MASK (MI_BLOCK_SIZE - 1)

// Bitstream profiles indicated by 2-3 bits in the uncompressed header.
// 00: Profile 0.  8-bit 4:2:0 only.
// 10: Profile 1.  8-bit 4:4:4, 4:2:2, and 4:4:0.
// 01: Profile 2.  10-bit and 12-bit color only, with 4:2:0 sampling.
// 110: Profile 3. 10-bit and 12-bit color only, with 4:2:2/4:4:4/4:4:0
//                 sampling.
// 111: Undefined profile.
typedef enum BITSTREAM_PROFILE {
  PROFILE_0,
  PROFILE_1,
  PROFILE_2,
  PROFILE_3,
  MAX_PROFILES
} BITSTREAM_PROFILE;

#define BLOCK_4X4 0
#define BLOCK_4X8 1
#define BLOCK_8X4 2
#define BLOCK_8X8 3
#define BLOCK_8X16 4
#define BLOCK_16X8 5
#define BLOCK_16X16 6
#define BLOCK_16X32 7
#define BLOCK_32X16 8
#define BLOCK_32X32 9
#define BLOCK_32X64 10
#define BLOCK_64X32 11
#define BLOCK_64X64 12
#define BLOCK_SIZES 13
#define BLOCK_INVALID BLOCK_SIZES
typedef uint8_t BLOCK_SIZE;

typedef enum PARTITION_TYPE {
  PARTITION_NONE,
  PARTITION_HORZ,
  PARTITION_VERT,
  PARTITION_SPLIT,
  PARTITION_TYPES,
  PARTITION_INVALID = PARTITION_TYPES
} PARTITION_TYPE;

typedef char PARTITION_CONTEXT;
#define PARTITION_PLOFFSET 4  // number of probability models per block size
#define PARTITION_CONTEXTS (4 * PARTITION_PLOFFSET)

// block transform size
typedef uint8_t TX_SIZE;
#define TX_4X4 ((TX_SIZE)0)    // 4x4 transform
#define TX_8X8 ((TX_SIZE)1)    // 8x8 transform
#define TX_16X16 ((TX_SIZE)2)  // 16x16 transform
#define TX_32X32 ((TX_SIZE)3)  // 32x32 transform
#define TX_SIZES ((TX_SIZE)4)

// frame transform mode
typedef enum {
  ONLY_4X4 = 0,        // only 4x4 transform used
  ALLOW_8X8 = 1,       // allow block transform size up to 8x8
  ALLOW_16X16 = 2,     // allow block transform size up to 16x16
  ALLOW_32X32 = 3,     // allow block transform size up to 32x32
  TX_MODE_SELECT = 4,  // transform specified for each block
  TX_MODES = 5,
} TX_MODE;

typedef enum {
  DCT_DCT = 0,    // DCT  in both horizontal and vertical
  ADST_DCT = 1,   // ADST in vertical, DCT in horizontal
  DCT_ADST = 2,   // DCT  in vertical, ADST in horizontal
  ADST_ADST = 3,  // ADST in both directions
  TX_TYPES = 4
} TX_TYPE;

#define EXT_TX_SIZES 3  // number of sizes that use extended transforms

typedef enum {
  AOM_LAST_FLAG = 1 << 0,
#if CONFIG_EXT_REFS
  AOM_LAST2_FLAG = 1 << 1,
  AOM_LAST3_FLAG = 1 << 2,
  AOM_GOLD_FLAG = 1 << 3,
  AOM_BWD_FLAG = 1 << 4,
  AOM_ALT_FLAG = 1 << 5,
  AOM_REFFRAME_ALL = (1 << 6) - 1
#else
  AOM_GOLD_FLAG = 1 << 1,
  AOM_ALT_FLAG = 1 << 2,
  AOM_REFFRAME_ALL = (1 << 3) - 1
#endif  // CONFIG_EXT_REFS
} AOM_REFFRAME;

typedef enum { PLANE_TYPE_Y = 0, PLANE_TYPE_UV = 1, PLANE_TYPES } PLANE_TYPE;

#define DC_PRED 0    // Average of above and left pixels
#define V_PRED 1     // Vertical
#define H_PRED 2     // Horizontal
#define D45_PRED 3   // Directional 45  deg = round(arctan(1/1) * 180/pi)
#define D135_PRED 4  // Directional 135 deg = 180 - 45
#define D117_PRED 5  // Directional 117 deg = 180 - 63
#define D153_PRED 6  // Directional 153 deg = 180 - 27
#define D207_PRED 7  // Directional 207 deg = 180 + 27
#define D63_PRED 8   // Directional 63  deg = round(arctan(2/1) * 180/pi)
#define TM_PRED 9    // True-motion
#define NEARESTMV 10
#define NEARMV 11
#define ZEROMV 12
#define NEWMV 13
#define MB_MODE_COUNT 14
typedef uint8_t PREDICTION_MODE;

#define INTRA_MODES (TM_PRED + 1)

#define INTER_MODES (1 + NEWMV - NEARESTMV)

#if CONFIG_EXT_INTRA
// all intra modes except DC and TM
#define DIRECTIONAL_MODES (INTRA_MODES - 2)
#endif  // CONFIG_EXT_INTRA

#if CONFIG_MOTION_VAR
typedef enum {
  SIMPLE_TRANSLATION = 0,  // regular block based motion compensation
  OBMC_CAUSAL = 1,         // 2-sided overlapped block prediction
  MOTION_MODES = 2
} MOTION_MODE;
#endif  // CONFIG_MOTION_VAR

#define SKIP_CONTEXTS 3

#if CONFIG_REF_MV
#define NMV_CONTEXTS 3

#define NEWMV_MODE_CONTEXTS 7
#define ZEROMV_MODE_CONTEXTS 2
#define REFMV_MODE_CONTEXTS 9
#define DRL_MODE_CONTEXTS 5

#define ZEROMV_OFFSET 3
#define REFMV_OFFSET 4

#define NEWMV_CTX_MASK ((1 << ZEROMV_OFFSET) - 1)
#define ZEROMV_CTX_MASK ((1 << (REFMV_OFFSET - ZEROMV_OFFSET)) - 1)
#define REFMV_CTX_MASK ((1 << (8 - REFMV_OFFSET)) - 1)

#define ALL_ZERO_FLAG_OFFSET 8
#define SKIP_NEARESTMV_OFFSET 9
#define SKIP_NEARMV_OFFSET 10
#define SKIP_NEARESTMV_SUB8X8_OFFSET 11
#endif

#define INTER_MODE_CONTEXTS 7

/* Segment Feature Masks */
#define MAX_MV_REF_CANDIDATES 2

#if CONFIG_REF_MV
#define MAX_REF_MV_STACK_SIZE 16
#define REF_CAT_LEVEL 160
#endif

#define INTRA_INTER_CONTEXTS 4
#define COMP_INTER_CONTEXTS 5
#define REF_CONTEXTS 5

// Reference frame types
#define NONE -1
#define INTRA_FRAME 0
#define LAST_FRAME 1

#if CONFIG_EXT_REFS
#define LAST2_FRAME 2
#define LAST3_FRAME 3
#define GOLDEN_FRAME 4
#define BWDREF_FRAME 5
#define ALTREF_FRAME 6
#define MAX_REF_FRAMES 7
#define LAST_REF_FRAMES (LAST3_FRAME - LAST_FRAME + 1)
#else
#define GOLDEN_FRAME 2
#define ALTREF_FRAME 3
#define MAX_REF_FRAMES 4
#endif  // CONFIG_EXT_REFS

#define FWD_REFS (GOLDEN_FRAME - LAST_FRAME + 1)
#define FWD_RF_OFFSET(ref) (ref - LAST_FRAME)

#if CONFIG_EXT_REFS
#define BWD_REFS (ALTREF_FRAME - BWDREF_FRAME + 1)
#define BWD_RF_OFFSET(ref) (ref - BWDREF_FRAME)
#else
#define BWD_REFS 1
#define BWD_RF_OFFSET(ref) (ref - ALTREF_FRAME)
#endif
#define SINGLE_REFS (FWD_REFS + BWD_REFS)
#define COMP_REFS (FWD_REFS * BWD_REFS)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ENUMS_H_

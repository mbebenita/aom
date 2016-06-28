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

#ifndef AV1_ENCODER_BLOCK_H_
#define AV1_ENCODER_BLOCK_H_

#include "av1/common/entropymv.h"
#include "av1/common/entropy.h"
#if CONFIG_PVQ
#include "av1/encoder/encint.h"
#endif
#if CONFIG_REF_MV
#include "av1/common/mvref_common.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned int sse;
  int sum;
  unsigned int var;
} diff;

struct macroblock_plane {
  DECLARE_ALIGNED(16, int16_t, src_diff[64 * 64]);
  DECLARE_ALIGNED(16, int16_t, src_int16[64 * 64]);
  tran_low_t *qcoeff;
  tran_low_t *coeff;
  uint16_t *eobs;
  struct buf_2d src;

  // Quantizer setings
  int16_t *quant_fp;
  int16_t *round_fp;
  int16_t *quant;
  int16_t *quant_shift;
  int16_t *zbin;
  int16_t *round;

  int64_t quant_thred[2];
};

/* The [2] dimension is for whether we skip the EOB node (i.e. if previous
 * coefficient in this block was zero) or not. */
typedef unsigned int av1_coeff_cost[PLANE_TYPES][REF_TYPES][COEF_BANDS][2]
                                   [COEFF_CONTEXTS][ENTROPY_TOKENS];

typedef struct {
  int_mv ref_mvs[MODE_CTX_REF_FRAMES][MAX_MV_REF_CANDIDATES];
  int16_t mode_context[MODE_CTX_REF_FRAMES];
#if CONFIG_REF_MV
  uint8_t ref_mv_count[MODE_CTX_REF_FRAMES];
  CANDIDATE_MV ref_mv_stack[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
#endif
} MB_MODE_INFO_EXT;

typedef struct macroblock MACROBLOCK;
struct macroblock {
  struct macroblock_plane plane[MAX_MB_PLANE];

  MACROBLOCKD e_mbd;
  MB_MODE_INFO_EXT *mbmi_ext;
  int skip_block;
  int select_tx_size;
  int q_index;

  // The equivalent error at the current rdmult of one whole bit (not one
  // bitcost unit).
  int errorperbit;
  // The equivalend SAD error of one (whole) bit at the current quantizer
  // for large blocks.
  int sadperbit16;
  // The equivalend SAD error of one (whole) bit at the current quantizer
  // for sub-8x8 blocks.
  int sadperbit4;
  int rddiv;
  int rdmult;
  int mb_energy;
  int *m_search_count_ptr;
  int *ex_search_count_ptr;

  // These are set to their default values at the beginning, and then adjusted
  // further in the encoding process.
  BLOCK_SIZE min_partition_size;
  BLOCK_SIZE max_partition_size;

  int mv_best_ref_index[MAX_REF_FRAMES];
  unsigned int max_mv_context[MAX_REF_FRAMES];
  unsigned int source_variance;
  unsigned int pred_sse[MAX_REF_FRAMES];
  int pred_mv_sad[MAX_REF_FRAMES];

#if CONFIG_REF_MV
  int *nmvjointcost;
  int nmv_vec_cost[NMV_CONTEXTS][MV_JOINTS];
  int *nmvcost[NMV_CONTEXTS][2];
  int *nmvcost_hp[NMV_CONTEXTS][2];
  int **mv_cost_stack[NMV_CONTEXTS];
  int *nmvjointsadcost;
#else
  int nmvjointcost[MV_JOINTS];
  int *nmvcost[2];
  int *nmvcost_hp[2];
  int nmvjointsadcost[MV_JOINTS];
#endif
  int **mvcost;

  int *nmvsadcost[2];
  int *nmvsadcost_hp[2];
  int **mvsadcost;

  // These define limits to motion vector components to prevent them
  // from extending outside the UMV borders
  int mv_col_min;
  int mv_col_max;
  int mv_row_min;
  int mv_row_max;

  // Notes transform blocks where no coefficents are coded.
  // Set during mode selection. Read during block encoding.
  uint8_t zcoeff_blk[TX_SIZES][256];

  int skip;

  int encode_breakout;

  // note that token_costs is the cost when eob node is skipped
  av1_coeff_cost token_costs[TX_SIZES];

  int optimize;

  // indicate if it is in the rd search loop or encoding process
  int use_lp32x32fdct;

  // use fast quantization process
  int quant_fp;

  // skip forward transform and quantization
  uint8_t skip_txfm[MAX_MB_PLANE << 2];
#define SKIP_TXFM_NONE 0
#define SKIP_TXFM_AC_DC 1
#define SKIP_TXFM_AC_ONLY 2

  int64_t bsse[MAX_MB_PLANE << 2];

  // Used to store sub partition's choices.
  MV pred_mv[MAX_REF_FRAMES];

#if CONFIG_PVQ
  int rate;
  int64_t dist;
  PVQ_QUEUE *pvq_q;
  PVQ_INFO pvq[256][3]; // 16x16 of 4x4 blocks, YUV
  daala_enc_ctx daala_enc;
#endif
};

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_BLOCK_H_

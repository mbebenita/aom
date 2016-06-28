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

#ifndef AV1_COMMON_BLOCKD_H_
#define AV1_COMMON_BLOCKD_H_

#include "./aom_config.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"
#include "aom_scale/yv12config.h"

#include "av1/common/common_data.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/mv.h"
#if CONFIG_AOM_QM
#include "av1/common/quant_common.h"
#endif
#include "av1/common/scale.h"
#include "av1/common/seg_common.h"
#include "av1/common/tile_common.h"
#if CONFIG_PVQ
#include "av1/common/pvq.h"
#include "av1/common/state.h"
#include "av1/decoder/decint.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MB_PLANE 3

#if CONFIG_EXT_INTRA
#define MAX_ANGLE_DELTA 3
#define MAX_ANGLE_DELTA_UV 2
#define ANGLE_STEP_UV 4

static const uint8_t av1_angle_step_y[TX_SIZES][INTRA_MODES] = {
  {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      0, 4, 4, 4, 4, 4, 4, 4, 4, 0,
  },
  {
      0, 3, 3, 3, 3, 3, 3, 3, 3, 0,
  },
  {
      0, 3, 3, 3, 3, 3, 3, 3, 3, 0,
  },
};
static const uint8_t av1_max_angle_delta_y[TX_SIZES][INTRA_MODES] = {
  {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  },
  {
      0, 2, 2, 2, 2, 2, 2, 2, 2, 0,
  },
  {
      0, 3, 3, 3, 3, 3, 3, 3, 3, 0,
  },
  {
      0, 3, 3, 3, 3, 3, 3, 3, 3, 0,
  },
};
static const uint8_t mode_to_angle_map[INTRA_MODES] = {
  0, 90, 180, 45, 135, 111, 157, 203, 67, 0,
};

static INLINE int is_directional_mode(PREDICTION_MODE mode) {
  return (mode < TM_PRED && mode != DC_PRED);
}
#endif  // CONFIG_EXT_INTRA

typedef enum {
  KEY_FRAME = 0,
  INTER_FRAME = 1,
  FRAME_TYPES,
} FRAME_TYPE;

static INLINE int is_inter_mode(PREDICTION_MODE mode) {
  return mode >= NEARESTMV && mode <= NEWMV;
}

#if CONFIG_PVQ
typedef struct PVQ_INFO {
  int theta[PVQ_MAX_PARTITIONS];
  int max_theta[PVQ_MAX_PARTITIONS];
  int qg[PVQ_MAX_PARTITIONS];
  int k[PVQ_MAX_PARTITIONS];
  od_coeff y[OD_BSIZE_MAX*OD_BSIZE_MAX];
  int nb_bands;
  int off[PVQ_MAX_PARTITIONS];
  int size[PVQ_MAX_PARTITIONS];
  int skip_rest;
  int skip_dir;
  int bs;         // log of the block size minus two,
                  // i.e. equivalent to aom's TX_SIZE
  int ac_dc_coded;// block skip info, indicating whether DC/AC is coded.
                  // bit0: DC coded, bit1 : AC coded (1 means coded)
  tran_low_t dq_dc_residue;
  int eob;
} PVQ_INFO;

typedef struct PVQ_QUEUE {
  PVQ_INFO *buf; // buffer for pvq info, stored in encoding order
  int curr_pos; // curr position to write PVQ_INFO
  int buf_len;  // allocated buffer length
  int last_pos; // last written position of PVQ_INFO in a tile
} PVQ_QUEUE;
#endif

/* For keyframes, intra block modes are predicted by the (already decoded)
   modes for the Y blocks to the left and above us; for interframes, there
   is a single probability table. */

typedef struct {
  PREDICTION_MODE as_mode;
  int_mv as_mv[2];  // first, second inter predictor motion vectors
#if CONFIG_REF_MV
  int_mv pred_mv[2];
#endif
} b_mode_info;

typedef int8_t MV_REFERENCE_FRAME;

#if CONFIG_REF_MV
#define MODE_CTX_REF_FRAMES (MAX_REF_FRAMES + COMP_REFS)
#else
#define MODE_CTX_REF_FRAMES MAX_REF_FRAMES
#endif

// This structure now relates to 8x8 block regions.
typedef struct {
  // Common for both INTER and INTRA blocks
  BLOCK_SIZE sb_type;
  PREDICTION_MODE mode;
  TX_SIZE tx_size;
  int8_t skip;
#if CONFIG_MISC_FIXES
  int8_t has_no_coeffs;
#endif
  int8_t segment_id;
  int8_t seg_id_predicted;  // valid only when temporal_update is enabled

  // Only for INTRA blocks
  PREDICTION_MODE uv_mode;
#if CONFIG_EXT_INTRA
  // The actual prediction angle is the base angle + (angle_delta * step).
  int8_t intra_angle_delta[2];
#endif  // CONFIG_EXT_INTRA

  // Only for INTER blocks
  InterpFilter interp_filter;
  MV_REFERENCE_FRAME ref_frame[2];
#if CONFIG_MOTION_VAR
  MOTION_MODE motion_mode;
#endif  // CONFIG_MOTION_VAR
  TX_TYPE tx_type;

#if CONFIG_REF_MV
  int_mv pred_mv[2];
  uint8_t ref_mv_idx;
#endif
  // TODO(slavarnway): Delete and use bmi[3].as_mv[] instead.
  int_mv mv[2];
  /* deringing gain *per-superblock* */
  int8_t dering_gain;
} MB_MODE_INFO;

typedef struct MODE_INFO {
  MB_MODE_INFO mbmi;
  b_mode_info bmi[4];
} MODE_INFO;

static INLINE PREDICTION_MODE get_y_mode(const MODE_INFO *mi, int block) {
  return mi->mbmi.sb_type < BLOCK_8X8 ? mi->bmi[block].as_mode : mi->mbmi.mode;
}

static INLINE int is_inter_block(const MB_MODE_INFO *mbmi) {
  return mbmi->ref_frame[0] > INTRA_FRAME;
}

static INLINE int has_second_ref(const MB_MODE_INFO *mbmi) {
  return mbmi->ref_frame[1] > INTRA_FRAME;
}

PREDICTION_MODE av1_left_block_mode(const MODE_INFO *cur_mi,
                                    const MODE_INFO *left_mi, int b);

PREDICTION_MODE av1_above_block_mode(const MODE_INFO *cur_mi,
                                     const MODE_INFO *above_mi, int b);

enum mv_precision { MV_PRECISION_Q3, MV_PRECISION_Q4 };

struct buf_2d {
  uint8_t *buf;
  int stride;
};

struct macroblockd_plane {
  tran_low_t *dqcoeff;
  PLANE_TYPE plane_type;
  int subsampling_x;
  int subsampling_y;
  struct buf_2d dst;
  struct buf_2d pre[2];
  ENTROPY_CONTEXT *above_context;
  ENTROPY_CONTEXT *left_context;
  int16_t seg_dequant[MAX_SEGMENTS][2];
  uint8_t *color_index_map;

  // number of 4x4s in current block
  uint16_t n4_w, n4_h;
  // log2 of n4_w, n4_h
  uint8_t n4_wl, n4_hl;

#if CONFIG_AOM_QM
  const qm_val_t *seg_iqmatrix[MAX_SEGMENTS][2][TX_SIZES];
#endif
  // encoder
  const int16_t *dequant;

#if CONFIG_AOM_QM
  const qm_val_t *seg_qmatrix[MAX_SEGMENTS][2][TX_SIZES];
#endif

#if CONFIG_PVQ
  DECLARE_ALIGNED(16, int16_t, pred[64 * 64]);
  // PVQ: forward transformed predicted image, a reference for PVQ.
  tran_low_t *pvq_ref_coeff;
#endif
};

#define BLOCK_OFFSET(x, i) ((x) + (i)*16)

typedef struct RefBuffer {
  // TODO(dkovalev): idx is not really required and should be removed, now it
  // is used in av1_onyxd_if.c
  int idx;
  YV12_BUFFER_CONFIG *buf;
  struct scale_factors sf;
} RefBuffer;

typedef struct macroblockd {
  struct macroblockd_plane plane[MAX_MB_PLANE];
  uint8_t bmode_blocks_wl;
  uint8_t bmode_blocks_hl;

  FRAME_COUNTS *counts;
  TileInfo tile;

  int mi_stride;

  MODE_INFO **mi;
  MODE_INFO *left_mi;
  MODE_INFO *above_mi;
  MB_MODE_INFO *left_mbmi;
  MB_MODE_INFO *above_mbmi;

  int up_available;
  int left_available;

  /* Distance of MB away from frame edges */
  int mb_to_left_edge;
  int mb_to_right_edge;
  int mb_to_top_edge;
  int mb_to_bottom_edge;

  uint8_t n8_w, n8_h;

  FRAME_CONTEXT *fc;

  /* pointers to reference frames */
  RefBuffer *block_refs[2];

  /* pointer to current frame */
  const YV12_BUFFER_CONFIG *cur_buf;

#if CONFIG_REF_MV
  uint8_t ref_mv_count[MODE_CTX_REF_FRAMES];
  CANDIDATE_MV ref_mv_stack[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
  uint8_t is_sec_rect;
#endif

  ENTROPY_CONTEXT *above_context[MAX_MB_PLANE];
  ENTROPY_CONTEXT left_context[MAX_MB_PLANE][16];

  PARTITION_CONTEXT *above_seg_context;
  PARTITION_CONTEXT left_seg_context[8];

#if CONFIG_PVQ
  daala_dec_ctx daala_dec;
#endif
#if CONFIG_AOM_HIGHBITDEPTH
  /* Bit depth: 8, 10, 12 */
  int bd;
#endif

  int lossless[MAX_SEGMENTS];
  int corrupted;

  struct aom_internal_error_info *error_info;
} MACROBLOCKD;

static INLINE BLOCK_SIZE get_subsize(BLOCK_SIZE bsize,
                                     PARTITION_TYPE partition) {
  return subsize_lookup[partition][bsize];
}

static const TX_TYPE intra_mode_to_tx_type_context[INTRA_MODES] = {
  DCT_DCT,    // DC
  ADST_DCT,   // V
  DCT_ADST,   // H
  DCT_DCT,    // D45
  ADST_ADST,  // D135
  ADST_DCT,   // D117
  DCT_ADST,   // D153
  DCT_ADST,   // D207
  ADST_DCT,   // D63
  ADST_ADST,  // TM
};

static INLINE TX_TYPE get_tx_type(PLANE_TYPE plane_type, const MACROBLOCKD *xd,
                                  int block_idx) {
  const MODE_INFO *const mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;

  (void)block_idx;
  if (plane_type != PLANE_TYPE_Y || xd->lossless[mbmi->segment_id] ||
      mbmi->tx_size >= TX_32X32)
    return DCT_DCT;

  return mbmi->tx_type;
}

void av1_setup_block_planes(MACROBLOCKD *xd, int ss_x, int ss_y);

static INLINE TX_SIZE get_uv_tx_size_impl(TX_SIZE y_tx_size, BLOCK_SIZE bsize,
                                          int xss, int yss) {
  if (bsize < BLOCK_8X8) {
    return TX_4X4;
  } else {
    const BLOCK_SIZE plane_bsize = ss_size_lookup[bsize][xss][yss];
    return AOMMIN(y_tx_size, max_txsize_lookup[plane_bsize]);
  }
}

static INLINE TX_SIZE get_uv_tx_size(const MB_MODE_INFO *mbmi,
                                     const struct macroblockd_plane *pd) {
  return get_uv_tx_size_impl(mbmi->tx_size, mbmi->sb_type, pd->subsampling_x,
                             pd->subsampling_y);
}

static INLINE BLOCK_SIZE
get_plane_block_size(BLOCK_SIZE bsize, const struct macroblockd_plane *pd) {
  return ss_size_lookup[bsize][pd->subsampling_x][pd->subsampling_y];
}

static INLINE void reset_skip_context(MACROBLOCKD *xd, BLOCK_SIZE bsize) {
  int i;
  for (i = 0; i < MAX_MB_PLANE; i++) {
    struct macroblockd_plane *const pd = &xd->plane[i];
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
    memset(pd->above_context, 0,
           sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide_lookup[plane_bsize]);
    memset(pd->left_context, 0,
           sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high_lookup[plane_bsize]);
  }
}

#if CONFIG_MOTION_VAR
static INLINE int is_motion_variation_allowed_bsize(BLOCK_SIZE bsize) {
  return (bsize >= BLOCK_8X8);
}

static INLINE int is_motion_variation_allowed(const MB_MODE_INFO *mbmi) {
  return is_motion_variation_allowed_bsize(mbmi->sb_type);
}

static INLINE int is_neighbor_overlappable(const MB_MODE_INFO *mbmi) {
  return (is_inter_block(mbmi));
}
#endif  // CONFIG_MOTION_VAR

typedef void (*foreach_transformed_block_visitor)(int plane, int block,
                                                  int blk_row, int blk_col,
                                                  BLOCK_SIZE plane_bsize,
                                                  TX_SIZE tx_size, void *arg);

void av1_foreach_transformed_block_in_plane(
    const MACROBLOCKD *const xd, BLOCK_SIZE bsize, int plane,
    foreach_transformed_block_visitor visit, void *arg);

void av1_foreach_transformed_block(const MACROBLOCKD *const xd,
                                   BLOCK_SIZE bsize,
                                   foreach_transformed_block_visitor visit,
                                   void *arg);

void av1_set_contexts(const MACROBLOCKD *xd, struct macroblockd_plane *pd,
                      BLOCK_SIZE plane_bsize, TX_SIZE tx_size, int has_eob,
                      int aoff, int loff);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_BLOCKD_H_

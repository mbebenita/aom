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

#ifndef AV1_ENCODER_CONTEXT_TREE_H_
#define AV1_ENCODER_CONTEXT_TREE_H_

#include "av1/common/blockd.h"
#include "av1/encoder/block.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AV1_COMP;
struct AV1Common;
struct ThreadData;

// Structure to hold snapshot of coding context during the mode picking process
typedef struct {
  MODE_INFO mic;
  MB_MODE_INFO_EXT mbmi_ext;
  uint8_t *zcoeff_blk;
  uint8_t *color_index_map[2];
  tran_low_t *coeff[MAX_MB_PLANE][3];
  tran_low_t *qcoeff[MAX_MB_PLANE][3];
  tran_low_t *dqcoeff[MAX_MB_PLANE][3];
#if CONFIG_PVQ
  tran_low_t *pvq_ref_coeff[MAX_MB_PLANE][3];
#endif
  uint16_t *eobs[MAX_MB_PLANE][3];

  // dual buffer pointers, 0: in use, 1: best in store
  tran_low_t *coeff_pbuf[MAX_MB_PLANE][3];
  tran_low_t *qcoeff_pbuf[MAX_MB_PLANE][3];
  tran_low_t *dqcoeff_pbuf[MAX_MB_PLANE][3];
#if CONFIG_PVQ
  tran_low_t *pvq_ref_coeff_pbuf[MAX_MB_PLANE][3];
#endif
  uint16_t *eobs_pbuf[MAX_MB_PLANE][3];

  int is_coded;
  int num_4x4_blk;
  int skip;
  int pred_pixel_ready;
  // For current partition, only if all Y, U, and V transform blocks'
  // coefficients are quantized to 0, skippable is set to 0.
  int skippable;
  int best_mode_index;
  int hybrid_pred_diff;
  int comp_pred_diff;
  int single_pred_diff;

  // TODO(jingning) Use RD_COST struct here instead. This involves a boarder
  // scope of refactoring.
  int rate;
  int64_t dist;

  // motion vector cache for adaptive motion search control in partition
  // search loop
  MV pred_mv[MAX_REF_FRAMES];
  InterpFilter pred_interp_filter;
} PICK_MODE_CONTEXT;

typedef struct PC_TREE {
  int index;
  PARTITION_TYPE partitioning;
  BLOCK_SIZE block_size;
  PICK_MODE_CONTEXT none;
  PICK_MODE_CONTEXT horizontal[2];
  PICK_MODE_CONTEXT vertical[2];
  union {
    struct PC_TREE *split[4];
    PICK_MODE_CONTEXT *leaf_split[4];
  };
} PC_TREE;

void av1_setup_pc_tree(struct AV1Common *cm, struct ThreadData *td);
void av1_free_pc_tree(struct ThreadData *td);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* AV1_ENCODER_CONTEXT_TREE_H_ */
